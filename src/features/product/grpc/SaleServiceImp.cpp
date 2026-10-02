#include "SaleServiceImpl.h"

#include <exception>
#include <string>
#include <unordered_set>
#include <variant>

#include "features/product/domain/Sale.h"

namespace puntodeventa::v1 {

SaleServiceImpl::SaleServiceImpl(
    ::puntodeventa::sale::SaleRepository& repository
)
    : repository_(repository)
{
}


grpc::Status SaleServiceImpl::CreateSale(
    grpc::ServerContext* context,
    const CreateSaleRequest* request,
    CreateSaleResponse* response
) {

    /*
     * 1. Comprobar cancelación antes de comenzar.
     *
     * No volvemos a comprobar IsCancelled() después
     * del commit, porque para entonces la venta podría
     * haberse persistido correctamente.
     */
    if (context->IsCancelled()) {

        return grpc::Status{
            grpc::StatusCode::CANCELLED,
            "Petición cancelada"
        };
    }


    /*
     * 2. Validar request_id.
     *
     * request_id representa una operación lógica
     * concreta y permite hacer CreateSale idempotente.
     */
    if (request->request_id().empty()) {

        return grpc::Status{
            grpc::StatusCode::INVALID_ARGUMENT,
            "request_id no puede estar vacío"
        };
    }


    /*
     * 3. Validar que el carrito no esté vacío.
     */
    if (request->items().empty()) {

        return grpc::Status{
            grpc::StatusCode::INVALID_ARGUMENT,
            "La venta debe contener al menos un producto"
        };
    }


    /*
     * 4. Detectar barcodes repetidos dentro
     *    de la misma petición.
     *
     * Ejemplo inválido:
     *
     * [
     *   { barcode: "123", cantidad: 1 },
     *   { barcode: "123", cantidad: 2 }
     * ]
     */
    std::unordered_set<std::string> barcodes;

    barcodes.reserve(
        static_cast<std::size_t>(
            request->items_size()
        )
    );


    for (const auto& item : request->items()) {

        const auto [iterator, inserted] =
            barcodes.insert(
                item.barcode()
            );


        if (!inserted) {

            return grpc::Status{
                grpc::StatusCode::INVALID_ARGUMENT,
                "Barcode repetido dentro del carrito: "
                    + item.barcode()
            };
        }
    }


    /*
     * 5. Convertir protobuf -> modelo de dominio.
     */
    ::puntodeventa::sale::SaleRequest saleRequest;


    /*
     * request_id.
     */
    saleRequest.request_id =
        request->request_id();


    /*
     * PaymentMethod Proto -> Domain.
     */
    switch (request->payment_method()) {

        case PaymentMethod::PAYMENT_METHOD_DEBIT:

            saleRequest.payment_method =
                ::puntodeventa::sale::PaymentMethod::Debit;

            break;


        case PaymentMethod::PAYMENT_METHOD_CREDIT:

            saleRequest.payment_method =
                ::puntodeventa::sale::PaymentMethod::Credit;

            break;


        default:

            return grpc::Status{
                grpc::StatusCode::INVALID_ARGUMENT,
                "Método de pago inválido"
            };
    }


    /*
     * Reservar espacio para evitar realocaciones
     * innecesarias.
     */
    saleRequest.items.reserve(
        static_cast<std::size_t>(
            request->items_size()
        )
    );


    /*
     * SaleItemRequest Proto -> Domain.
     */
    for (const auto& item : request->items()) {

        saleRequest.items.push_back(
            ::puntodeventa::sale::SaleItemRequest{
                .barcode =
                    item.barcode(),

                .cantidad =
                    item.cantidad(),

                .precio_unitario =
                    item.precio_unitario()
            }
        );
    }


    try {

        /*
         * 6. Ejecutar la operación.
         *
         * La idempotencia se resuelve dentro
         * del repository utilizando request_id.
         *
         * Si llega dos veces exactamente:
         *
         * request_id = ABC
         *
         * la segunda llamada recuperará la venta
         * original y NO volverá a descontar stock.
         */
        const auto result =
            repository_.create(
                saleRequest
            );


        /*
         * 7. Manejar errores de dominio.
         */
        if (
            std::holds_alternative<
                ::puntodeventa::sale::SaleFailure
            >(result)
        ) {

            const auto& failure =
                std::get<
                    ::puntodeventa::sale::SaleFailure
                >(result);


            using ::puntodeventa::sale::SaleError;


            switch (failure.error) {

                /*
                 * Venta vacía.
                 */
                case SaleError::EmptySale:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "La venta está vacía"
                    };


                /*
                 * request_id inválido.
                 */
                case SaleError::InvalidRequestId:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "request_id inválido"
                    };


                /*
                 * Cantidad <= 0.
                 */
                case SaleError::InvalidQuantity:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Cantidad inválida para el producto: "
                            + failure.barcode
                    };


                /*
                 * Precio inválido.
                 */
                case SaleError::InvalidPrice:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Precio inválido para el producto: "
                            + failure.barcode
                    };


                /*
                 * Conflicto de precio de dominio.
                 */
                case SaleError::ConflictingPrice:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Precio conflictivo para el producto: "
                            + failure.barcode
                    };


                /*
                 * Barcode repetido.
                 *
                 * Normalmente ya fue detectado arriba,
                 * pero mantenemos esta protección porque
                 * el repository también valida.
                 */
                case SaleError::DuplicateBarcode:

                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Barcode repetido dentro del carrito: "
                            + failure.barcode
                    };


                /*
                 * Producto inexistente o inactivo.
                 */
                case SaleError::ProductNotFound:

                    return grpc::Status{
                        grpc::StatusCode::NOT_FOUND,
                        "Producto no encontrado: "
                            + failure.barcode
                    };


                /*
                 * No hay suficiente inventario.
                 */
                case SaleError::InsufficientStock:

                    return grpc::Status{
                        grpc::StatusCode::FAILED_PRECONDITION,
                        "Stock insuficiente para el producto: "
                            + failure.barcode
                    };


                /*
                 * Se intentó reutilizar un request_id
                 * para una operación diferente.
                 *
                 * Ejemplo:
                 *
                 * request_id ABC:
                 * Libreta x1 @ $50
                 *
                 * después:
                 *
                 * request_id ABC:
                 * Libreta x1 @ $40
                 *
                 * Eso NO es un retry válido.
                 */
                case SaleError::IdempotencyConflict:

                    return grpc::Status{
                        grpc::StatusCode::ALREADY_EXISTS,
                        "El request_id ya fue utilizado "
                        "para una venta diferente"
                    };
            }


            /*
             * Protección adicional.
             *
             * Si en el futuro agregamos un SaleError
             * y olvidamos mapearlo arriba, no queremos
             * continuar como si hubiera una Sale válida.
             */
            return grpc::Status{
                grpc::StatusCode::INTERNAL,
                "Error de venta no reconocido"
            };
        }


        /*
         * 8. Obtener la venta creada o la venta
         *    existente recuperada por idempotencia.
         */
        const auto& sale =
            std::get<
                ::puntodeventa::sale::Sale
            >(result);


        /*
         * 9. Construir respuesta protobuf.
         */
        response->set_ok(
            true
        );


        /*
         * Importante para que el cliente pueda
         * correlacionar la respuesta con la petición.
         */
        response->set_request_id(
            sale.request_id
        );


        response->set_sale_id(
            sale.sale_id
        );


        response->set_total(
            sale.total
        );


        /*
         * Construir snapshot de los productos vendidos.
         */
        for (const auto& item : sale.items) {

            auto* responseItem =
                response->add_items();


            responseItem->set_product_id(
                item.product_id
            );


            responseItem->set_barcode(
                item.barcode
            );


            responseItem->set_nombre(
                item.nombre
            );


            responseItem->set_cantidad(
                item.cantidad
            );


            responseItem->set_precio_unitario(
                item.precio_unitario
            );


            responseItem->set_subtotal(
                item.subtotal
            );
        }


        /*
         * IMPORTANTE:
         *
         * NO comprobamos:
         *
         * context->IsCancelled()
         *
         * después de repository_.create().
         *
         * Para este punto pudo haber ocurrido COMMIT.
         *
         * Si el cliente perdió la conexión después
         * del COMMIT, la venta existe.
         *
         * Cuando el cliente reintente con el mismo
         * request_id, la protección de idempotencia
         * devolverá la venta original sin volver a
         * descontar stock.
         */
        return grpc::Status::OK;
    }


    /*
     * 10. Errores inesperados.
     */
    catch (const std::exception& exception) {

        return grpc::Status{
            grpc::StatusCode::INTERNAL,
            exception.what()
        };
    }


    catch (...) {

        return grpc::Status{
            grpc::StatusCode::INTERNAL,
            "Error interno desconocido al crear la venta"
        };
    }
}

} // namespace puntodeventa::v1
