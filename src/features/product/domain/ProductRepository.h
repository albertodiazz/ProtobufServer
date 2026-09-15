#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "features/product/domain/Product.h"


namespace puntodeventa::product {


	class ProductRepository {
		public:
			virtual ~ProductRepository() = default;

			virtual int64_t create(const Producto& producto) = 0;

			virtual std::optional<Producto> update(const Producto& producto) = 0;

			virtual std::vector<ProductoResumen> listProducts(
						std::int32_t limit,
						std::optional<std::int64_t> beforeId
					) = 0; 

			virtual std::optional<Producto> getByBarcode(
					const std::string& barcode
					) = 0;

	};

}

