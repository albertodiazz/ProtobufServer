#pragma once

#include <string>


namespace puntodeventa::product {

	struct Producto {
		std::string nombre; 
		std::string barcode;
		std::string descripcion; 
		int32_t precio;
		int32_t costo;
		std::string image_key;
	};

}
