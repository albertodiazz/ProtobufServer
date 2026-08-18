#include "CreateProduct.h"

#include <stdexcept>
#include <string>
#include <iostream>

#include "core/database/S3ObjectStorage/OjectStorage.h"
#include "core/image/ImageFormatDetector.h"
#include "core/image/ImageFormat.h"
#include "features/BarcodeGenerator.h"
#include "features/product/domain/Product.h"

namespace puntodeventa::product {

CreateProduct::CreateProduct(
    ProductRepository& repository,
    ::puntodeventa::storage::ObjectStorage& objectStorage
)
    : repository_(repository),
      objectStorage_(objectStorage)
{
}

CreateProductResult CreateProduct::execute(
    const CreateProductCommand& command
) {
    // 1. Generar barcode
    const std::string barcode =
        ::puntodeventa::generadorBarcode();

    std::string imageKey;

		// 2. Procesar imagen si existe
		if (command.imagen.has_value()) {

			std::cerr << "[IMAGE 1] optional presente\n";

			const std::string& imageData =
				command.imagen.value();

			std::cerr << "[IMAGE 2] bytes: "
				<< imageData.size()
				<< '\n';

			if (imageData.empty()) {
				throw std::invalid_argument(
						"La imagen esta vacia"
						);
			}

			std::cerr << "[IMAGE 3] antes de detectImageFormat\n";

			const auto format =
				::puntodeventa::image::detectImageFormat(
						imageData
						);

			std::cerr << "[IMAGE 4] despues de detectImageFormat\n";

			std::string extension;
			std::string contentType;

			switch (format) {

				case ::puntodeventa::image::ImageFormat::PNG:
					extension = ".png";
					contentType = "image/png";
					break;

				case ::puntodeventa::image::ImageFormat::JPEG:
					extension = ".jpg";
					contentType = "image/jpeg";
					break;

				case ::puntodeventa::image::ImageFormat::WEBP:
					extension = ".webp";
					contentType = "image/webp";
					break;

				default:
					throw std::invalid_argument(
							"Formato de imagen no soportado"
							);
			}

			std::cerr << "[IMAGE 5] formato reconocido\n";
			std::cerr << "[IMAGE 6] antes de putObject\n";

			imageKey = objectStorage_.putObject(
					"products/" +
					barcode +
					"/main" +
					extension,
					imageData,
					contentType
					);

			std::cerr << "[IMAGE 7] despues de putObject\n";
		}

		// 4. Construir entidad de dominio
		Producto producto{
			.nombre = command.nombre,
				.barcode = barcode,
				.descripcion = command.descripcion,
				.precio = command.precio,
				.costo = command.costo,
				.image_key = imageKey
		};

		// 5. Persistir producto
		const int64_t productoId =
			repository_.create(producto);

		// 6. Regresar resultado del caso de uso
		return CreateProductResult{
			.productId = productoId,
				.barcode = barcode
		};
}

}
