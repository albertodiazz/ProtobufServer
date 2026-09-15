#pragma once

#include <string>
#include <cstdint>


namespace puntodeventa::product {

	struct Producto {
		std::string nombre; 
		std::string barcode;
		std::string descripcion; 
		int32_t precio;
		int32_t costo;
		std::string image_key;
		std::string thumbnail_key;
	};


	struct ProductoResumen {
		std::int64_t product_id = 0;
		std::string nombre; 
		std::string barcode;
		int32_t precio = 0;
		std::string thumbnail_key;
	};


}
