#include "SqlProductRepository.h"
#include <optional>
#include <iostream>

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
								image_key
            )
            VALUES ($1, $2, $3, $4, $5, $6)
            RETURNING id
        )",
			pqxx::params{
			producto.nombre,
			producto.descripcion,
			producto.precio,
			producto.costo,
			producto.barcode,
			producto.image_key
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
                        image_key = $5
                    WHERE barcode = $6
                    RETURNING
                        nombre,
                        barcode,
                        descripcion,
                        precio,
                        costo,
                        image_key
                )",
                pqxx::params{
                    producto.nombre,
                    producto.descripcion,
                    producto.precio,
                    producto.costo,
                    producto.image_key,
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
            .image_key = row["image_key"].as<std::string>()
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
                image_key
            FROM products
            WHERE barcode = $1
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
		: row["image_key"].as<std::string>()
	};

	return producto;
}

}
