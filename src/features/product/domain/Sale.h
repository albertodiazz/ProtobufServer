// Sale.h

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

namespace puntodeventa::sale {

enum class PaymentMethod {
    Debit,
    Credit
};

struct SaleItemRequest {
    std::string barcode;
    std::int32_t cantidad;
    std::int32_t precio_unitario;
};

struct SaleRequest {
    std::string request_id;
    PaymentMethod payment_method;
    std::vector<SaleItemRequest> items;
};

struct SaleItem {
    std::int64_t product_id;
    std::string barcode;
    std::string nombre;
    std::int32_t cantidad;
    std::int32_t precio_unitario;
    std::int64_t subtotal;
};

struct Sale {
    std::int64_t sale_id;

    std::string request_id;

    PaymentMethod payment_method;

    std::int64_t total;

    std::vector<SaleItem> items;
};

enum class SaleError {
    EmptySale,
    InvalidRequestId,
    InvalidQuantity,
    InvalidPrice,
    DuplicateBarcode,
    ProductNotFound,
    InsufficientStock,
		ConflictingPrice,

    // El mismo request_id se intentó utilizar
    // para una venta diferente.
    IdempotencyConflict
};

struct SaleFailure {
    SaleError error;
    std::string barcode;
};

using CreateSaleResult =
    std::variant<
        Sale,
        SaleFailure
    >;

}
