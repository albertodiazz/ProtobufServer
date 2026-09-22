#pragma once

#include <pqxx/pqxx>

#include "features/product/domain/SaleRepository.h"

namespace puntodeventa::sale {

class SqlSaleRepository final : public SaleRepository {

public:
    explicit SqlSaleRepository(
        pqxx::connection& connection
    );

    CreateSaleResult create(
        const SaleRequest& sale
    ) override;

private:
    pqxx::connection& connection_;
};

}
