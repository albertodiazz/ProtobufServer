#include "SqlSaleRepository.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace puntodeventa::sale {

namespace {

std::string paymentMethodToDatabase(
    PaymentMethod paymentMethod
) {
    switch (paymentMethod) {

        case PaymentMethod::Debit:
            return "DEBIT";

        case PaymentMethod::Credit:
            return "CREDIT";
    }

    throw std::invalid_argument(
        "PaymentMethod desconocido"
    );
}


PaymentMethod paymentMethodFromDatabase(
    const std::string& paymentMethod
) {
    if (paymentMethod == "DEBIT") {
        return PaymentMethod::Debit;
    }

    if (paymentMethod == "CREDIT") {
        return PaymentMethod::Credit;
    }

    throw std::runtime_error(
        "Método de pago inválido almacenado en la BD"
    );
}

}


SqlSaleRepository::SqlSaleRepository(
    pqxx::connection& connection
)
    : connection_(connection)
{
}


CreateSaleResult SqlSaleRepository::create(
    const SaleRequest& sale
) {

    /*
     * 1. Validaciones básicas antes de abrir
     *    la transacción.
     */

    if (sale.request_id.empty()) {
        return SaleFailure{
            SaleError::InvalidRequestId,
            ""
        };
    }


    if (sale.items.empty()) {
        return SaleFailure{
            SaleError::EmptySale,
            ""
        };
    }


    for (const auto& item : sale.items) {

        if (item.cantidad <= 0) {
            return SaleFailure{
                SaleError::InvalidQuantity,
                item.barcode
            };
        }

        if (item.precio_unitario < 0) {
            return SaleFailure{
                SaleError::InvalidPrice,
                item.barcode
            };
        }
    }


    /*
     * 2. Ordenamos por barcode.
     *
     * Esto tiene dos objetivos:
     *
     * - detectar barcodes repetidos;
     * - adquirir locks siempre en el mismo orden para
     *   reducir posibilidades de deadlock.
     */
    std::vector<SaleItemRequest> items =
        sale.items;

    std::sort(
        items.begin(),
        items.end(),
        [](
            const SaleItemRequest& a,
            const SaleItemRequest& b
        ) {
            return a.barcode < b.barcode;
        }
    );


    /*
     * Protección adicional.
     *
     * Aunque SaleServiceImpl ya debería rechazar esto,
     * el repository también se protege por si algún día
     * es utilizado desde otro punto.
     */
    const auto duplicate =
        std::adjacent_find(
            items.begin(),
            items.end(),
            [](
                const SaleItemRequest& a,
                const SaleItemRequest& b
            ) {
                return a.barcode == b.barcode;
            }
        );


    if (duplicate != items.end()) {
        return SaleFailure{
            SaleError::DuplicateBarcode,
            duplicate->barcode
        };
    }


    /*
     * 3. Toda la operación ocurre dentro de
     *    una sola transacción.
     */
    pqxx::work transaction{connection_};


    const std::string paymentMethod =
        paymentMethodToDatabase(
            sale.payment_method
        );


    /*
     * 4. Intentamos crear la venta.
     *
     * request_id tiene una restricción UNIQUE.
     *
     * ON CONFLICT evita el clásico:
     *
     * SELECT -> no existe
     *
     * otro request:
     * SELECT -> no existe
     *
     * ambos:
     * INSERT
     *
     * PostgreSQL es quien tiene la última palabra.
     */
    const pqxx::result insertedSale =
        transaction.exec(
            R"(
                INSERT INTO sales (
                    request_id,
                    payment_method,
                    total
                )
                VALUES (
                    $1,
                    $2,
                    0
                )

                ON CONFLICT (request_id)
                DO NOTHING

                RETURNING id
            )",
            pqxx::params{
                sale.request_id,
                paymentMethod
            }
        );


    /*
     * 5. Si INSERT no devolvió fila significa que
     *    request_id ya existe.
     *
     * Esto representa un retry de una operación
     * posiblemente ya completada.
     */
    if (insertedSale.empty()) {

        /*
         * Recuperar cabecera de la venta original.
         */
        const pqxx::row existingSaleRow =
            transaction.exec(
                R"(
                    SELECT
                        id,
                        request_id,
                        payment_method,
                        total
                    FROM sales
                    WHERE request_id = $1
                )",
                pqxx::params{
                    sale.request_id
                }
            ).one_row();


        const std::int64_t existingSaleId =
            existingSaleRow["id"]
                .as<std::int64_t>();

        const std::string existingPaymentString =
            existingSaleRow["payment_method"]
                .as<std::string>();

        const PaymentMethod existingPaymentMethod =
            paymentMethodFromDatabase(
                existingPaymentString
            );


        /*
         * Recuperamos los items históricos.
         *
         * ORDER BY barcode permite compararlos con
         * el request actual que ya ordenamos arriba.
         */
        const pqxx::result existingItemsResult =
            transaction.exec(
                R"(
                    SELECT
                        product_id,
                        barcode,
                        nombre,
                        cantidad,
                        precio_unitario,
                        subtotal
                    FROM sale_items
                    WHERE sale_id = $1
                    ORDER BY barcode ASC
                )",
                pqxx::params{
                    existingSaleId
                }
            );


        /*
         * 6. El mismo request_id solamente puede
         *    representar exactamente la misma operación.
         *
         * Si alguien reutilizó accidentalmente el UUID
         * para otro carrito, es un conflicto.
         */
        if (
            existingPaymentMethod !=
            sale.payment_method
        ) {
            transaction.commit();

            return SaleFailure{
                SaleError::IdempotencyConflict,
                ""
            };
        }


        if (
            existingItemsResult.size() !=
            items.size()
        ) {
            transaction.commit();

            return SaleFailure{
                SaleError::IdempotencyConflict,
                ""
            };
        }


        for (
            std::size_t i = 0;
            i < items.size();
            ++i
        ) {

            const auto& requestedItem =
                items[i];

            const auto& existingItem =
                existingItemsResult[i];


            const std::string existingBarcode =
                existingItem["barcode"]
                    .as<std::string>();

            const std::int32_t existingCantidad =
                existingItem["cantidad"]
                    .as<std::int32_t>();

            const std::int32_t existingPrecio =
                existingItem["precio_unitario"]
                    .as<std::int32_t>();


            if (
                requestedItem.barcode !=
                    existingBarcode
                ||
                requestedItem.cantidad !=
                    existingCantidad
                ||
                requestedItem.precio_unitario !=
                    existingPrecio
            ) {

                transaction.commit();

                return SaleFailure{
                    SaleError::IdempotencyConflict,
                    ""
                };
            }
        }


        /*
         * 7. Es exactamente la misma venta.
         *
         * NO:
         *
         * - descontamos stock;
         * - insertamos sale_items;
         * - creamos otra venta.
         *
         * Simplemente reconstruimos y devolvemos
         * la venta original.
         */
        Sale existingSale;

        existingSale.sale_id =
            existingSaleId;

        existingSale.request_id =
            sale.request_id;

        existingSale.payment_method =
            existingPaymentMethod;

        existingSale.total =
            existingSaleRow["total"]
                .as<std::int64_t>();


        existingSale.items.reserve(
            existingItemsResult.size()
        );


        for (const auto& row : existingItemsResult) {

            SaleItem soldItem;

            soldItem.product_id =
                row["product_id"]
                    .as<std::int64_t>();

            soldItem.barcode =
                row["barcode"]
                    .as<std::string>();

            soldItem.nombre =
                row["nombre"]
                    .as<std::string>();

            soldItem.cantidad =
                row["cantidad"]
                    .as<std::int32_t>();

            soldItem.precio_unitario =
                row["precio_unitario"]
                    .as<std::int32_t>();

            soldItem.subtotal =
                row["subtotal"]
                    .as<std::int64_t>();


            existingSale.items.push_back(
                std::move(soldItem)
            );
        }


        transaction.commit();

        return existingSale;
    }


    /*
     * 8. Si llegamos aquí, acabamos de crear
     *    una venta nueva.
     */
    const std::int64_t saleId =
        insertedSale
            .one_row()["id"]
            .as<std::int64_t>();


    std::int64_t total = 0;

    std::vector<SaleItem> soldItems;

    soldItems.reserve(
        items.size()
    );


    /*
     * 9. Procesar cada producto.
     */
    for (const auto& item : items) {

        /*
         * Bloqueamos el producto.
         *
         * Nadie más puede modificar esta fila
         * hasta que nuestra transacción termine.
         */
        const pqxx::result productResult =
            transaction.exec(
                R"(
                    SELECT
                        id,
                        nombre,
                        barcode,
                        cantidad
                    FROM products
                    WHERE barcode = $1
                    FOR UPDATE
                )",
                pqxx::params{
                    item.barcode
                }
            );


        /*
         * Producto inexistente.
         *
         * Como todavía no hubo commit:
         *
         * - desaparece sales;
         * - desaparecen sale_items anteriores;
         * - se revierten cantidades.
         */
        if (productResult.empty()) {
            return SaleFailure{
                SaleError::ProductNotFound,
                item.barcode
            };
        }


        const auto& productRow =
            productResult[0];


        const std::int64_t productId =
            productRow["id"]
                .as<std::int64_t>();

        const std::string nombre =
            productRow["nombre"]
                .as<std::string>();

        const std::int32_t stock =
            productRow["cantidad"]
                .as<std::int32_t>();


        /*
         * 10. Validar inventario.
         */
        if (stock < item.cantidad) {
            return SaleFailure{
                SaleError::InsufficientStock,
                item.barcode
            };
        }


        /*
         * 11. Calcular subtotal.
         */
        const std::int64_t subtotal =
            static_cast<std::int64_t>(
                item.cantidad
            )
            *
            static_cast<std::int64_t>(
                item.precio_unitario
            );


        /*
         * Protección de overflow del total.
         */
        if (
            subtotal >
            std::numeric_limits<
                std::int64_t
            >::max() - total
        ) {
            throw std::overflow_error(
                "El total de la venta excede int64_t"
            );
        }


        total += subtotal;


        /*
         * 12. Descontar inventario.
         */
        transaction.exec(
            R"(
                UPDATE products
                SET cantidad = cantidad - $1
                WHERE id = $2
            )",
            pqxx::params{
                item.cantidad,
                productId
            }
        );


        /*
         * 13. Guardar snapshot histórico
         *     del producto vendido.
         */
        transaction.exec(
            R"(
                INSERT INTO sale_items (
                    sale_id,
                    product_id,
                    barcode,
                    nombre,
                    cantidad,
                    precio_unitario,
                    subtotal
                )
                VALUES (
                    $1,
                    $2,
                    $3,
                    $4,
                    $5,
                    $6,
                    $7
                )
            )",
            pqxx::params{
                saleId,
                productId,
                item.barcode,
                nombre,
                item.cantidad,
                item.precio_unitario,
                subtotal
            }
        );


        /*
         * 14. Construir respuesta.
         */
        SaleItem soldItem;

        soldItem.product_id =
            productId;

        soldItem.barcode =
            item.barcode;

        soldItem.nombre =
            nombre;

        soldItem.cantidad =
            item.cantidad;

        soldItem.precio_unitario =
            item.precio_unitario;

        soldItem.subtotal =
            subtotal;


        soldItems.push_back(
            std::move(soldItem)
        );
    }


    /*
     * 15. Guardar total definitivo.
     */
    transaction.exec(
        R"(
            UPDATE sales
            SET total = $1
            WHERE id = $2
        )",
        pqxx::params{
            total,
            saleId
        }
    );


    /*
     * 16. Solamente aquí queda confirmada
     *     toda la venta.
     */
    transaction.commit();


    /*
     * 17. Respuesta de dominio.
     */
    Sale completedSale;

    completedSale.sale_id =
        saleId;

    completedSale.request_id =
        sale.request_id;

    completedSale.payment_method =
        sale.payment_method;

    completedSale.total =
        total;

    completedSale.items =
        std::move(soldItems);


    return completedSale;
}

}
