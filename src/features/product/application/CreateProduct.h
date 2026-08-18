#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "features/product/domain/ProductRepository.h"
#include "core/database/S3ObjectStorage/OjectStorage.h"

namespace puntodeventa::product {

struct CreateProductCommand {
    std::string nombre;
    std::string descripcion;
    int32_t precio;
    int32_t costo;
    std::optional<std::string> imagen;
};

struct CreateProductResult {
    int64_t productId;
    std::string barcode;
};

class CreateProduct {
public:
    CreateProduct(
        ProductRepository& repository,
        ::puntodeventa::storage::ObjectStorage& objectStorage
    );

    CreateProductResult execute(
        const CreateProductCommand& command
    );

private:
    ProductRepository& repository_;
    ::puntodeventa::storage::ObjectStorage& objectStorage_;
};

}
