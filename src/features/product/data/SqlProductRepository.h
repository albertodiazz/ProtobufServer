#pragma once

#include <pqxx/pqxx>

#include "features/product/domain/ProductRepository.h"

namespace puntodeventa::product {

class SqlProductRepository final : public ProductRepository {
public:
    explicit SqlProductRepository(
        pqxx::connection& connection
    );

    int64_t create(
        const Producto& producto
    ) override;

private:
    pqxx::connection& connection_;
};

}
