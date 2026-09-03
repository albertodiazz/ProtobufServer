#pragma once

#include <optional>
#include <string>
#include "core/image/ImageFormatDetector.h"

namespace puntodeventa::v1 {

	enum class ProductValidationError {
		EmptyName,
		InvalidPrice, // No pueden ser negativos ni < 0
		InvalidCost, // No pueden ser negativos ni < 0
		EmptyImage,
		InvalidFormatImage
	};

	class ProductValidator {

		public:

			static std::optional<ProductValidationError> validate(
					const std::string& nombre,
					const std::string& descripcion,
					int64_t precio,
					int64_t costo,
					const std::string& image,
					image::ImageFormat formato_imagen,
					std::string& extension,
					std::string& contentType
					);

			static std::string validationErrorMessage(
					ProductValidationError error
					);

	};

}

