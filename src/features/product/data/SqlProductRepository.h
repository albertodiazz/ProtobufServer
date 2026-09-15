#pragma once

#include <optional>
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

			std::optional<Producto> update(
					const Producto& producto 
					) override;

			std::vector<ProductoResumen> listProducts(
					std::int32_t limit,
					std::optional<std::int64_t> beforeId
					) override;

			std::optional<Producto>	 getByBarcode(
					const std::string& barcode 
					) override;

		private:
			pqxx::connection& connection_;
	};

}
