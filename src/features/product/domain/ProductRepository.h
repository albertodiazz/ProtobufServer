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

			/*
			 * Busca productos activos utilizando PostgreSQL
			 * Full Text Search sobre nombre + descripcion.
			 *
			 * Todos los términos significativos son obligatorios
			 * porque plainto_tsquery utiliza AND.
			 */
			virtual SearchProductsPage searchProducts(
					const std::string& query,
					std::int32_t limit,
					std::optional<ProductSearchCursor> cursor
					) = 0;

			virtual std::optional<Producto> getByBarcode(
					const std::string& barcode
					) = 0;

			virtual bool deleteByBarcode(
					const std::string& barcode
					) = 0;

	};

}

