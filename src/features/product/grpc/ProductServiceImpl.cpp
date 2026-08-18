#include "ProductServiceImpl.h"

#include <exception>
#include <grpcpp/support/status.h>
#include <optional>
#include <stdexcept>

#include "punto_de_venta.pb.h"
#include "features/BarcodeGenerator.h"
#include "features/product/domain/Product.h"
#include "core/image/ImageFormatDetector.h"

#include <string>

namespace {
	void toProto(
			const puntodeventa::product::Producto& source,
			puntodeventa::v1::Producto* target
			) {
		target->set_nombre(source.nombre);
		target->set_barcode(source.barcode);
		target->set_descripcion(source.descripcion);
		target->set_precio(source.precio);
		target->set_costo(source.costo);
	}
}

namespace puntodeventa::v1 {

	ProductServiceImpl::ProductServiceImpl(
			::puntodeventa::product::ProductRepository& repository,
			::puntodeventa::storage::ObjectStorage& objectStorage
			)
		: repository_(repository),
		objectStorage_(objectStorage)
	{
	}



	grpc::Status ProductServiceImpl::CreateProduct(
			grpc::ServerContext* context,
			const CreateProductRequest* request,
			CreateProductResponse* response
			) {
		std::cout << "[1] Inicio CreateProduct\n";

		std::string barcode =
			puntodeventa::generadorBarcode();

		std::cout << "[2] Barcode generado: "
			<< barcode << '\n';

		std::string imageKey;

		if (request->has_imagen()) {

			std::cout << "[3] Imagen presente\n";

			const std::string& imageData =
				request->imagen().data();

			std::cout << "[4] Imagen size: "
				<< imageData.size()
				<< " bytes\n";

			if (imageData.empty()) {
				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"La imagen esta vacia"
				};
			}

			const auto format =
				puntodeventa::image::detectImageFormat(imageData);

			std::cout << "[5] Formato detectado\n";

			std::string extension;
			std::string contentType;

			switch (format) {
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
					return grpc::Status{
						grpc::StatusCode::INVALID_ARGUMENT,
							"Formato de imagen no soportado"
					};
			}

			std::cout << "[6] Antes de S3\n";

			imageKey = objectStorage_.putObject(
					"products/" +
					barcode +
					"/main" +
					extension,
					imageData,
					contentType
					);

			std::cout << "[7] S3 terminado. Key: "
				<< imageKey << '\n';
		}

		puntodeventa::product::Producto producto{
			.nombre = request->nombre(),
				.barcode = barcode,
				.descripcion = request->descripcion(),
				.precio = request->precio(),
				.costo = request->costo(),
				.image_key = imageKey
		};

		std::cout << "[8] Antes de PostgreSQL\n";

		const int64_t productoId =
			repository_.create(producto);

		std::cout << "[9] Producto guardado ID: "
			<< productoId << '\n';

		response->set_ok(true);
		response->set_mensaje(
				"Producto recibido correctamente"
				);
		response->set_product_id(productoId);
		response->set_internal_barcode(barcode);

		std::cout << "[10] RPC terminado\n";

		return grpc::Status::OK;
	}


	grpc::Status ProductServiceImpl::GetProductById(
			grpc::ServerContext* context,
			const GetProductByIdRequest* request,
			GetProductResponse* response
			) {
		return grpc::Status{
			grpc::StatusCode::UNIMPLEMENTED,
			"GetProductById no implementado"
		};
	}

	grpc::Status ProductServiceImpl::GetProductByBarcode(
			grpc::ServerContext* context,
			const GetProductByBarcodeRequest* request,
			GetProductResponse* response
			) {

		const std::string& barcode = 
			request->barcode();

		const auto producto = 
			repository_.getByBarcode(barcode);

		if(!producto.has_value()){
			return grpc::Status{
				grpc::StatusCode::NOT_FOUND,
					"Producto no encontrado"
			};
		}

		toProto(
				*producto,
				response->mutable_producto()
				);


		const auto imagenData = 
			objectStorage_.getObject(producto->image_key);

		// Que pasa si no tiene valor la imagen?
		response->mutable_producto()->mutable_imagen()->set_data(imagenData);

		return grpc::Status{
			grpc::StatusCode::OK,
				"Producto encontrado"
		};
	}

	grpc::Status ProductServiceImpl::UpdateProduct(
			grpc::ServerContext* context,
			const UpdateProductRequest* request,
			UpdateProductResponse* response
			) {
		return grpc::Status{
			grpc::StatusCode::UNIMPLEMENTED,
			"UpdateProduct no implementado"
		};
	}

	grpc::Status ProductServiceImpl::DeleteProduct(
			grpc::ServerContext* context,
			const DeleteProductRequest* request,
			DeleteProductResponse* response
			) {
		return grpc::Status{
			grpc::StatusCode::UNIMPLEMENTED,
			"DeleteProduct no implementado"
		};
	}

}

