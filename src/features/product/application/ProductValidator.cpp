#include "ProductValidator.h"

namespace puntodeventa::v1 {

	std::optional<ProductValidationError>
		ProductValidator::validate(
				const std::string& nombre,
				const std::string& descripcion,
				int64_t precio,
				int64_t costo,
				const std::string& image,
				image::ImageFormat formato_imagen,
				std::string& extension,
				std::string& contentType
				) {

			if (nombre.empty()) {
				return ProductValidationError::EmptyName;
			}

			if (precio <= 0) {
				return ProductValidationError::InvalidPrice;
			}

			if (costo <= 0) {
				return ProductValidationError::InvalidCost;
			}

			if (image.empty()){
				return ProductValidationError::EmptyImage;
			}

			switch (formato_imagen) {
				case puntodeventa::image::ImageFormat::PNG:
					extension = ".png";
					contentType = "image/png";
					break;

				case puntodeventa::image::ImageFormat::JPEG:
					extension = ".jpg";
					contentType = "image/jpeg";
					break;

				case puntodeventa::image::ImageFormat::WEBP:
					extension = ".webp";
					contentType = "image/webp";
					break;

				default:
					return ProductValidationError::InvalidFormatImage;
			}

			return std::nullopt;
		}

	std::optional<ProductValidationError> 
		ProductValidator::validateBarCode(
				const std::string& barcode 
				){

			if(barcode.empty()){
				return ProductValidationError::EmpytBarCode;
			}

			if (barcode.length() != 13) {
				return ProductValidationError::InvalidBarCode;
			}

			if (barcode[0] == '0') {
				return ProductValidationError::InvalidBarCode;
			}

			for (char digit : barcode) {
				if (digit < '0' || digit > '9') {
					return ProductValidationError::InvalidBarCode;
				}
			}

			// Validar checksum EAN-13
			int suma = 0;
			for (int i = 0; i < 12; ++i) {
				const int digito = barcode[i] - '0';
				if (i % 2 == 0) {
					suma += digito;
				} else {
					suma += digito * 3;
				}
			}
			const int checksumEsperado =
				(10 - (suma % 10)) % 10;
			const int checksumRecibido =
				barcode[12] - '0';
			if (checksumEsperado != checksumRecibido) {
				return ProductValidationError::InvalidBarCode;
			}

			return std::nullopt;
		}

	std::string ProductValidator::validationErrorMessage(
			ProductValidationError error
			){
		switch(error) {

			case ProductValidationError::EmptyName:
				return "El nombre del producto es obligatorio";

			case ProductValidationError::InvalidPrice:
				return "El precio debe ser mayor a cero";

			case ProductValidationError::InvalidCost:
				return "El costo no puede ser negativo";

			case ProductValidationError::EmptyImage:
				return "La imagen esta vacia";

			case ProductValidationError::InvalidFormatImage:
				return "El formato de la imagen no es valido";

			case ProductValidationError::InvalidBarCode:
				return "El formato del Barcode es invalido debe ser EAN-13";

			case ProductValidationError::EmpytBarCode:
				return "El barcode no puede estar vacio";

		}

		return "Error de validacion"; 
	}


}
