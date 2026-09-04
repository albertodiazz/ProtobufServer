#include "ProductServiceImpl.h"

#include <exception>
#include <grpcpp/support/status.h>
#include <optional>
#include <stdexcept>

#include "punto_de_venta.pb.h"
#include "features/BarcodeGenerator.h"
#include "features/product/domain/Product.h"
#include "core/image/ImageFormatDetector.h"
#include "features/product/application/ProductValidator.h"

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

		std::string extension;
		std::string contentType;

		const std::string& imageData =
			request->imagen().data();

		puntodeventa::product::Producto producto{
			.nombre = request->nombre(),
				.barcode = barcode,
				.descripcion = request->descripcion(),
				.precio = request->precio(),
				.costo = request->costo(),
				.image_key = imageData 
		};

		const auto validation = 
			ProductValidator::validate(
					producto.nombre,
					producto.descripcion,
					producto.precio,
					producto.costo,
					producto.image_key,
					puntodeventa::image::detectImageFormat(producto.image_key),
					extension,
					contentType
					);

		if(validation){
			const std::string message = 
				ProductValidator::validationErrorMessage(*validation);
			std::cout << "Error: " << message << request->precio() << std::endl;
			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					message
			};
		}

		std::string imageKey;

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
		// Ahora guardamos la referencias de S3
		producto.image_key = imageKey;
		const int64_t productoId =
			repository_.create(producto);

		std::cout << "[9] Producto guardado ID: "
			<< productoId << '\n';

		response->set_ok(true);
		response->set_mensaje(
				"El producto fue guardado correctamente"
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
			){

		const auto& productoRequest = 
			request->producto();
		/*
		 * 1. Validar barcode
		 */
		const auto barcodeValidation =
			ProductValidator::validateBarCode(
					productoRequest.barcode()
					);

		if (barcodeValidation) {

			const std::string message =
				ProductValidator::validationErrorMessage(
						*barcodeValidation
						);

			std::cout
				<< "Error Barcode: "
				<< message
				<< '\n';

			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					message
			};
		}


		/*
		 * 2. Verificar que exista
		 */
		const auto productoExistente =
			repository_.getByBarcode(
					productoRequest.barcode()
					);

		if (!productoExistente) {
			return grpc::Status{
				grpc::StatusCode::NOT_FOUND,
					"Producto no encontrado"
			};
		}


		/*
		 * 3. Obtener imagen
		 */
		const std::string& imageData =
			productoRequest.imagen().data();

		std::string extension;
		std::string contentType;


		/*
		 * 4. Validar producto
		 */
		const auto validation =
			ProductValidator::validate(
					productoRequest.nombre(),
					productoRequest.descripcion(),
					productoRequest.precio(),
					productoRequest.costo(),
					imageData,
					puntodeventa::image::detectImageFormat(
						imageData
						),
					extension,
					contentType
					);

		if (validation) {

			const std::string message =
				ProductValidator::validationErrorMessage(
						*validation
						);

			std::cout
				<< "Error: "
				<< message
				<< '\n';

			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					message
			};
		}


		/*
		 * 5. Subir imagen
		 */
		const std::string imageKey =
			objectStorage_.putObject(
					"products/" +
					productoRequest.barcode() +
					"/main" +
					extension,
					imageData,
					contentType
					);

		std::cout
    << "[UPDATE-1] imageKey: "
    << imageKey
    << '\n';

		/*
		 * 6. Construir producto ya validado
		 */
		puntodeventa::product::Producto producto{
			.nombre = productoRequest.nombre(),
				.barcode = productoRequest.barcode(),
				.descripcion = productoRequest.descripcion(),
				.precio = productoRequest.precio(),
				.costo = productoRequest.costo(),
				.image_key = imageKey
		};

		std::cout << "[UPDATE-2] Antes de PostgreSQL\n";
		/*
		 * 7. Actualizar PostgreSQL
		 */
		const auto productoResponse =
			repository_.update(producto);

		std::cout << "[UPDATE-3] PostgreSQL regreso\n";

		if (!productoResponse) {
			std::cout << "[UPDATE-4] UPDATE no encontro producto\n";
			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible actualizar el producto"
			};
		}

		std::cout << "[UPDATE-5] Producto actualizado\n";
		/*
		 * 8. Response
		 */
		response->set_ok(true);

		std::cout << "[UPDATE-6] Respondiendo gRPC\n";

		return grpc::Status{
			grpc::StatusCode::OK,
				"Producto actualizado"
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

