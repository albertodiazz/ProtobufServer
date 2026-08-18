#include "SqlProductRepository.h"
#include <optional>

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
