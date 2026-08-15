#include "SqlProductRepository.h"

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
								barcode
            )
            VALUES ($1, $2, $3, $4, $5)
            RETURNING id
        )",
			pqxx::params{
			producto.nombre,
			producto.descripcion,
			producto.precio,
			producto.costo,
			producto.barcode
			}
			).one_row();

	const int64_t productId =
		row["id"].as<int64_t>();

	transaction.commit();

	return productId;
}

}
