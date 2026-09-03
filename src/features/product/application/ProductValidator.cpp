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

		}

		return "Error de validacion"; 
	}


}
