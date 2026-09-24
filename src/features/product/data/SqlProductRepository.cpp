#include "SqlProductRepository.h"
#include <optional>
#include <iostream>
#include <algorithm>

namespace puntodeventa::product {

	SqlProductRepository::SqlProductRepository(
			pqxx::connection& connection
			)
		: connection_(connection)
	{
	}

	int64_t SqlProductRepository::create(
			const Producto& producto
			) {

		pqxx::work transaction{connection_};

		pqxx::row row = transaction.exec(
				R"(
            INSERT INTO products (
                nombre,
                descripcion,
                precio,
                costo,
								barcode,
								image_key,
								thumbnail_key,
								cantidad
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
            RETURNING id
        )",
				pqxx::params{
				producto.nombre,
				producto.descripcion,
				producto.precio,
				producto.costo,
				producto.barcode,
				producto.image_key,
				producto.thumbnail_key,
				producto.cantidad
				}
		).one_row();

		const int64_t productId =
			row["id"].as<int64_t>();

		transaction.commit();

		return productId;
	}

	std::optional<Producto>
		SqlProductRepository::update(
				const Producto& producto
				) {
			try {

				std::cout << "[DB-UPDATE-1] creando transaction\n";

				pqxx::work transaction{connection_};

				std::cout << "[DB-UPDATE-2] transaction creada\n";

				const pqxx::result result =
					transaction.exec(
							R"(
                    UPDATE products 
                    SET
                        nombre = $1,
                        descripcion = $2,
                        precio = $3,
                        costo = $4,
                        image_key = $5,
                        thumbnail_key= $6,
												cantidad = $7
                    WHERE barcode = $8
										AND active = TRUE
                    RETURNING
                        nombre,
                        barcode,
                        descripcion,
                        precio,
                        costo,
                        image_key,
												thumbnail_key,
												cantidad
                )",
							pqxx::params{
								producto.nombre,
									producto.descripcion,
									producto.precio,
									producto.costo,
									producto.image_key,
									producto.thumbnail_key,
									producto.cantidad,
									producto.barcode
							}
				);

				std::cout
					<< "[DB-UPDATE-3] exec regreso. Rows: "
					<< result.size()
					<< '\n';

				if (result.empty()) {

					std::cout << "[DB-UPDATE-4] barcode no encontrado\n";

					transaction.commit();

					return std::nullopt;
				}

				std::cout << "[DB-UPDATE-5] leyendo row\n";

				const auto& row = result[0];

				Producto productoActualizado{
					.nombre = row["nombre"].as<std::string>(),
						.barcode = row["barcode"].as<std::string>(),
						.descripcion = row["descripcion"].as<std::string>(),
						.precio = row["precio"].as<int32_t>(),
						.costo = row["costo"].as<int32_t>(),
						.image_key = row["image_key"].as<std::string>(),
						.thumbnail_key = row["thumbnail_key"].as<std::string>(),
						.cantidad = row["cantidad"].as<int32_t>()
				};

				std::cout << "[DB-UPDATE-6] antes commit\n";

				transaction.commit();

				std::cout << "[DB-UPDATE-7] commit terminado\n";

				return productoActualizado;

			} catch (const std::exception& e) {

				std::cerr
					<< "[DB-UPDATE-ERROR] "
					<< e.what()
					<< '\n';

				throw;
			}
		}


	std::optional<Producto> SqlProductRepository::getByBarcode(
			const std::string& barcode
			) {
		pqxx::work tx{connection_};

		const pqxx::result result = tx.exec(
				R"(
            SELECT
                nombre,
                descripcion,
                precio,
                costo,
								barcode,
                image_key,
								cantidad
            FROM products
            WHERE barcode = $1
						AND active = TRUE
            LIMIT 1
        )",
				pqxx::params{barcode}
				);

		if (result.empty()) {
			return std::nullopt;
		}

		const auto& row = result[0];


		Producto producto{
			.nombre = row["nombre"].as<std::string>(),
				.barcode = row["barcode"].as<std::string>(),
				.descripcion = row["descripcion"].as<std::string>(),
				.precio = row["precio"].as<int32_t>(),
				.costo = row["costo"].as<int32_t>(),
				.image_key = row["image_key"].is_null()
					? std::string{}
			: row["image_key"].as<std::string>(),
				.cantidad = row["cantidad"].as<int32_t>()
		};

		return producto;
	}

	std::vector<ProductoResumen> SqlProductRepository::listProducts(
			std::int32_t limit,
			std::optional<std::int64_t> beforeId
			) {
		if (limit < 1 || limit > 101) {
			throw std::invalid_argument("limit debe estar entre 1 y 101");
		}

		if (beforeId && *beforeId <= 0) {
			throw std::invalid_argument("beforeId debe ser positivo");
		}

		pqxx::work transaction{connection_};

		const pqxx::result result = beforeId
			? transaction.exec(
					R"(
                SELECT
                    id,
                    nombre,
                    barcode,
                    precio,
										cantidad,
                    COALESCE(thumbnail_key, '') AS thumbnail_key
                FROM products
                WHERE id < $1
								AND active = TRUE
                ORDER BY id DESC
                LIMIT $2
            )",
					pqxx::params{*beforeId, limit}
					)
			: transaction.exec(
					R"(
                SELECT
                    id,
                    nombre,
                    barcode,
                    precio,
										cantidad,
                    COALESCE(thumbnail_key, '') AS thumbnail_key
                FROM products
								WHERE active = TRUE
                ORDER BY id DESC
                LIMIT $1
            )",
					pqxx::params{limit}
					);

		std::vector<ProductoResumen> productos;
		productos.reserve(result.size());

		for (const auto& row : result) {
			productos.push_back(ProductoResumen{
					.product_id = row["id"].as<std::int64_t>(),
					.nombre = row["nombre"].as<std::string>(),
					.barcode = row["barcode"].as<std::string>(),
					.precio = row["precio"].as<std::int32_t>(),
					.thumbnail_key = row["thumbnail_key"].as<std::string>(),
					.cantidad = row["cantidad"].as<std::int32_t>()
					});
		}

		transaction.commit();
		return productos;
	}

	SearchProductsPage
		SqlProductRepository::searchProducts(
				const std::string& query,
				std::int32_t limit,
				std::optional<ProductSearchCursor> cursor
				) {

			/*
			 * Una búsqueda vacía no produce resultados.
			 */
			if (query.empty()) {
				return {};
			}

			/*
			 * limit representa la cantidad máxima de productos
			 * que vamos a devolver al cliente.
			 *
			 * Internamente pediremos uno adicional para saber
			 * si existe una siguiente página.
			 */
			if (limit < 1 || limit > 100) {
				throw std::invalid_argument(
						"limit debe estar entre 1 y 100"
						);
			}

			/*
			 * Validar cursor si existe.
			 */
			if (cursor) {

				if (cursor->product_id <= 0) {
					throw std::invalid_argument(
							"cursor.product_id debe ser positivo"
							);
				}

				if (cursor->rank < 0.0f) {
					throw std::invalid_argument(
							"cursor.rank no puede ser negativo"
							);
				}
			}

			/*
			 * Ejemplo:
			 *
			 * limit = 20
			 * fetchLimit = 21
			 *
			 * El elemento adicional solamente nos dice
			 * si existe una página posterior.
			 */
			const std::int32_t fetchLimit =
				limit + 1;


			pqxx::read_transaction transaction{connection_};


			const pqxx::result result = cursor

				/*
				 * =================================================
				 * SIGUIENTE PÁGINA
				 * =================================================
				 */
				? transaction.exec(
						R"(
                WITH search AS (
                    SELECT
                        $1::TEXT AS texto,

                        plainto_tsquery(
                            'spanish',
                            $1
                        ) AS query
                ),

                ranked_products AS (
                    SELECT
                        p.id,
                        p.nombre,
                        p.barcode,
                        p.precio,

                        COALESCE(
                            p.thumbnail_key,
                            ''
                        ) AS thumbnail_key,

                        p.cantidad,

                        (
                            lower(p.nombre) =
                            lower(search.texto)
                        ) AS exact_match,

                        ts_rank(
                            p.search_vector,
                            search.query
                        ) AS rank

                    FROM products p

                    CROSS JOIN search

                    WHERE p.active = TRUE
                      AND p.search_vector @@ search.query
                )

                SELECT
                    id,
                    nombre,
                    barcode,
                    precio,
                    thumbnail_key,
                    cantidad,
                    exact_match,
                    rank

                FROM ranked_products

                WHERE (
                    exact_match,
                    rank,
                    id
                ) < (
                    $2::BOOLEAN,
                    $3::REAL,
                    $4::BIGINT
                )

                ORDER BY
                    exact_match DESC,
                    rank DESC,
                    id DESC

                LIMIT $5
				)",

				pqxx::params{
					query,
					cursor->exact_match,
					cursor->rank,
					cursor->product_id,
					fetchLimit
				}
			)

				/*
				 * =================================================
				 * PRIMERA PÁGINA
				 * =================================================
				 */
				: transaction.exec(
						R"(
                WITH search AS (
                    SELECT
                        $1::TEXT AS texto,

                        plainto_tsquery(
                            'spanish',
                            $1
                        ) AS query
                ),

                ranked_products AS (
                    SELECT
                        p.id,
                        p.nombre,
                        p.barcode,
                        p.precio,

                        COALESCE(
                            p.thumbnail_key,
                            ''
                        ) AS thumbnail_key,

                        p.cantidad,

                        (
                            lower(p.nombre) =
                            lower(search.texto)
                        ) AS exact_match,

                        ts_rank(
                            p.search_vector,
                            search.query
                        ) AS rank

                    FROM products p

                    CROSS JOIN search

                    WHERE p.active = TRUE
                      AND p.search_vector @@ search.query
                )

                SELECT
                    id,
                    nombre,
                    barcode,
                    precio,
                    thumbnail_key,
                    cantidad,
                    exact_match,
                    rank

                FROM ranked_products

                ORDER BY
                    exact_match DESC,
                    rank DESC,
                    id DESC

                LIMIT $2
            )",

				pqxx::params{
					query,
						fetchLimit
				}
			);


			SearchProductsPage page;


			/*
			 * Si pedimos 21 y recibimos 21:
			 *
			 * hay al menos otro elemento después de los
			 * 20 que vamos a devolver.
			 */

			const std::size_t resultSize =
				static_cast<std::size_t>(
						result.size()
						);

			const bool hasMore =
				resultSize >
				static_cast<std::size_t>(limit);

			/*
			 * Nunca entregamos el elemento adicional.
			 */
			const std::size_t productosADevolver =
				std::min(
						resultSize,
						static_cast<std::size_t>(limit)
						);

			page.productos.reserve(
					productosADevolver
					);


			/*
			 * Necesitamos recordar la metadata del último
			 * producto REALMENTE devuelto.
			 *
			 * Ese será nuestro next_cursor.
			 */
			ProductSearchCursor lastCursor;
			bool hasLastCursor = false;


			for (
					std::size_t i = 0;
					i < productosADevolver;
					++i
					) {

				const auto& row = result[i];


				ProductoResumen producto{
					.product_id =
						row["id"].as<std::int64_t>(),

						.nombre =
							row["nombre"].as<std::string>(),

						.barcode =
							row["barcode"].as<std::string>(),

						.precio =
							row["precio"].as<std::int32_t>(),

						.thumbnail_key =
							row["thumbnail_key"].as<std::string>(),

						.cantidad =
							row["cantidad"].as<std::int32_t>()
				};


				page.productos.push_back(
						std::move(producto)
						);


				/*
				 * Guardamos el cursor correspondiente a este
				 * producto.
				 *
				 * Después del loop contendrá los valores del
				 * último producto retornado.
				 */
				lastCursor = ProductSearchCursor{
					.exact_match =
						row["exact_match"].as<bool>(),

						.rank =
							row["rank"].as<float>(),

						.product_id =
							row["id"].as<std::int64_t>()
				};

				hasLastCursor = true;
			}


			/*
			 * Solamente existe next_cursor si sabemos que hay
			 * otra página.
			 */
			if (hasMore && hasLastCursor) {
				page.next_cursor =
					lastCursor;
			}


			return page;
		}

	bool SqlProductRepository::deleteByBarcode(
			const std::string& barcode
			) {

		pqxx::work transaction{connection_};


		/*
		 * 1. Buscar y bloquear el producto.
		 *
		 * Esto también evita que una venta concurrente
		 * modifique este mismo producto mientras
		 * decidimos qué tipo de borrado hacer.
		 */
		const pqxx::result productResult =
			transaction.exec(
					R"(
                SELECT id
                FROM products
                WHERE barcode = $1
                FOR UPDATE
            )",
					pqxx::params{
					barcode
					}
					);


		if (productResult.empty()) {
			return false;
		}


		const std::int64_t productId =
			productResult[0]["id"]
			.as<std::int64_t>();


		/*
		 * 2. Comprobar si existe historial de ventas.
		 */
		const pqxx::result saleResult =
			transaction.exec(
					R"(
                SELECT 1
                FROM sale_items
                WHERE product_id = $1
                LIMIT 1
            )",
					pqxx::params{
					productId
					}
					);


		const bool hasSales =
			!saleResult.empty();


		/*
		 * 3. Si ya fue vendido:
		 *    soft delete.
		 */
		if (hasSales) {

			transaction.exec(
					R"(
                UPDATE products
                SET active = FALSE
                WHERE id = $1
            )",
					pqxx::params{
					productId
					}
					);


			transaction.commit();

			return true;
		}


		/*
		 * 4. Si nunca fue vendido:
		 *    borrado físico.
		 */
		transaction.exec(
				R"(
            DELETE FROM products
            WHERE id = $1
        )",
				pqxx::params{
				productId
				}
				);


		transaction.commit();

		return true;
	}

}
