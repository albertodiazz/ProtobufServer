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


			CreateProductReservation
				reserveCreateProductRequest(
						const std::string& uuid,
						const std::string& requestHash,
						const std::string& proposedBarcode
						) override;


			std::int64_t createIdempotent(
					const Producto& producto,
					const std::string& uuid
					) override;

			std::optional<Producto> update(
					const Producto& producto 
					) override;

			std::vector<ProductoResumen> listProducts(
					std::int32_t limit,
					std::optional<std::int64_t> beforeId
					) override;

			SearchProductsPage searchProducts(
					const std::string& query,
					std::int32_t limit,
					std::optional<ProductSearchCursor> cursor
					) override;

			std::optional<Producto>	 getByBarcode(
					const std::string& barcode 
					) override;

			bool deleteByBarcode(
					const std::string& barcode
					) override;

		private:
			pqxx::connection& connection_;
	};

}
