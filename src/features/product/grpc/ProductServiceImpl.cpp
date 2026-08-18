#include "ProductServiceImpl.h"

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace puntodeventa::v1 {

	// grpc::Status ProductServiceImpl::CreateProduct(
	// 		grpc::ServerContext*,
	// 		const CreateProductRequest* request,
	// 		CreateProductResponse* response
			// ) {
		// try {

			// ::puntodeventa::product::CreateProductCommand command{
			// 	.nombre = request->nombre(),
			// 	.descripcion = request->descripcion(),
			// 	.precio = request->precio(),
			// 	.costo = request->costo(),
			// 	.imagen = std::nullopt
			// };

			// if (request->has_imagen()) {
			// 	command.imagen =
			// 		request->imagen().data();
			// }

			// const auto result =
			// 	createProduct_.execute(command);

			// response->set_ok(true);
			// response->set_mensaje(
			// 		"Producto recibido correctamente"
			// 		);
			// response->set_product_id(
			// 		result.productId
			// 		);
			// response->set_internal_barcode(
			// 		result.barcode
			// 		);

			// return grpc::Status::OK;

		// } catch (const std::invalid_argument& e) {

			// return grpc::Status{
			// 	grpc::StatusCode::INVALID_ARGUMENT,
			// 		e.what()
			// };

		// } catch (const std::exception& e) {

			// return grpc::Status{
			// 	grpc::StatusCode::INTERNAL,
			// 		e.what()
			// };
		// }
	// }

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
		return grpc::Status{
			grpc::StatusCode::UNIMPLEMENTED,
			"GetProductByBarcode no implementado"
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
