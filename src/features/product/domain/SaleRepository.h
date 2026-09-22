#pragma once

#include "features/product/domain/Sale.h"

namespace puntodeventa::sale {

    class SaleRepository {

        public:

            virtual ~SaleRepository() = default;

            /*
             * Crea una venta completa.
             *
             * La implementación SQL será responsable de:
             *
             * - crear sales
             * - validar/descontar stock
             * - crear sale_items
             * - calcular total
             * - hacer COMMIT
             *
             * Todo dentro de una sola transacción.
             */
            virtual CreateSaleResult create(
                    const SaleRequest& sale
                    ) = 0;
    };

}
