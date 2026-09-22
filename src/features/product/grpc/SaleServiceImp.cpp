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
     * 2. Validar que request_id exista.
     *
     * request_id representa la operación lógica de venta
     * y será utilizado por el repository para garantizar
     * idempotencia.
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
     * Aunque barcode sea UNIQUE en productos,
     * eso no impide que el cliente mande:
     *
     * [
     *   { barcode: "123", cantidad: 1 },
     *   { barcode: "123", cantidad: 2 }
     * ]
     *
     * Nosotros consideramos eso una petición inválida.
     */
    std::unordered_set<std::string> barcodes;

    barcodes.reserve(
        static_cast<std::size_t>(
            request->items_size()
        )
    );

    for (const auto& item : request->items()) {

        const auto [iterator, inserted] =
            barcodes.insert(item.barcode());

        if (!inserted) {
            return grpc::Status{
                grpc::StatusCode::INVALID_ARGUMENT,
                "Barcode repetido dentro del carrito: "
                    + item.barcode()
            };
        }
    }


    /*
     * 5. Convertir el mensaje protobuf
     *    al modelo de dominio.
     */
    ::puntodeventa::sale::SaleRequest saleRequest;

    saleRequest.request_id =
        request->request_id();

    saleRequest.items.reserve(
        static_cast<std::size_t>(
            request->items_size()
        )
    );


    for (const auto& item : request->items()) {

        saleRequest.items.push_back(
            ::puntodeventa::sale::SaleItemRequest{
                .barcode = item.barcode(),
                .cantidad = item.cantidad(),
                .precio_unitario = item.precio_unitario()
            }
        );
    }


    try {

        /*
         * 6. Ejecutar la operación.
         *
         * La idempotencia se resuelve dentro del
         * repository usando request_id.
         *
         * Si la misma petición llega dos veces:
         *
         * request_id = ABC
         * request_id = ABC
         *
         * la segunda operación debe recuperar la
         * venta existente y NO descontar stock otra vez.
         */
        const auto result =
            repository_.create(saleRequest);


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

                case SaleError::EmptySale:
                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "La venta está vacía"
                    };


                case SaleError::InvalidRequestId:
                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "request_id inválido"
                    };


                case SaleError::InvalidQuantity:
                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Cantidad inválida para el producto: "
                            + failure.barcode
                    };


                case SaleError::InvalidPrice:
                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Precio inválido para el producto: "
                            + failure.barcode
                    };


                case SaleError::ConflictingPrice:
                    return grpc::Status{
                        grpc::StatusCode::INVALID_ARGUMENT,
                        "Precio conflictivo para el producto: "
                            + failure.barcode
                    };


                case SaleError::ProductNotFound:
                    return grpc::Status{
                        grpc::StatusCode::NOT_FOUND,
                        "Producto no encontrado: "
                            + failure.barcode
                    };


                case SaleError::InsufficientStock:
                    return grpc::Status{
                        grpc::StatusCode::FAILED_PRECONDITION,
                        "Stock insuficiente para el producto: "
                            + failure.barcode
                    };
            }


            /*
             * Protección por si en el futuro agregamos
             * un SaleError y olvidamos manejarlo arriba.
             */
            return grpc::Status{
                grpc::StatusCode::INTERNAL,
                "Error de venta no reconocido"
            };
        }


        /*
         * 8. Obtener venta creada o venta existente
         *    recuperada por idempotencia.
         */
        const auto& sale =
            std::get<
                ::puntodeventa::sale::Sale
            >(result);


        /*
         * 9. Construir respuesta protobuf.
         */
        response->set_ok(true);

        response->set_sale_id(
            sale.sale_id
        );

        response->set_total(
            sale.total
        );


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
         * NO hacemos:
         *
         * if (context->IsCancelled()) ...
         *
         * aquí.
         *
         * repository_.create() pudo haber hecho COMMIT.
         *
         * Si el cliente canceló justo después del commit,
         * la venta YA EXISTE.
         *
         * El retry será protegido por request_id.
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
