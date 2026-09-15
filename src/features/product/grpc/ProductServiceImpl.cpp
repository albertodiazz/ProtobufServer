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
#include "core/image/ThumbailGenerator.h"

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
		std::string imageKey_thumbnail;

		std::cout << "[6] Antes de S3\n";

		ThumbnailGenerator thumbnailGenerator;
		const Thumbnail thumbnail = thumbnailGenerator.generate(imageData);

		imageKey = objectStorage_.putObject(
				"products/" +
				barcode +
				"/main" +
				extension,
				imageData,
				contentType
				);

		imageKey_thumbnail = objectStorage_.putObject(
				"products/" +
				barcode +
				"/thumb" +
				thumbnail.extension,
				thumbnail.data,
				contentType
				);

		std::cout << "[7] S3 terminado. Key: "
			<< imageKey << '\n';
		// Ahora guardamos la referencias de S3
		producto.image_key = imageKey;
		producto.thumbnail_key = imageKey_thumbnail;
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


		ThumbnailGenerator thumbnailGenerator;
		const Thumbnail thumbnail = thumbnailGenerator.generate(imageData);

		const std::string imageKey =
			objectStorage_.putObject(
					"products/" +
					productoRequest.barcode() +
					"/main" +
					extension,
					imageData,
					contentType
					);

		const std::string imageKey_thumbnail = objectStorage_.putObject(
				"products/" +
				productoRequest.barcode() +
				"/thumb" +
				thumbnail.extension,
				thumbnail.data,
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
				.image_key = imageKey,
				.thumbnail_key = imageKey_thumbnail
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

	grpc::Status ProductServiceImpl::ListProducts(
			grpc::ServerContext* context,
			const ListProductsRequest* request,
			ListProductsResponse* response
			) {
		if (request->page_size() < 0) {
			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
				"page_size no puede ser negativo"
			};
		}

		const std::int32_t pageSize = request->page_size() == 0
			? 20
			: std::min<std::int32_t>(request->page_size(), 100);

		// Primera página: sin cursor. Para continuar: último ID entregado.
		// Android debe reenviar el token recibido, sin calcularlo.
		std::optional<std::int64_t> beforeId;
		const std::string& token = request->page_token();

		if (!token.empty()) {
			if (token.size() > 19) {
				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"page_token inválido"
				};
			}

			std::int64_t id = 0;
			const auto parsed = std::from_chars(
					token.data(), token.data() + token.size(), id
					);

			if (parsed.ec != std::errc{} ||
					parsed.ptr != token.data() + token.size() || id <= 0) {
				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"page_token inválido"
				};
			}

			beforeId = id;
		}

		if (context->IsCancelled()) {
			return grpc::Status{
				grpc::StatusCode::CANCELLED,
					"Petición cancelada"
			};
		}

		try {
			// Una fila adicional permite detectar la siguiente página.
			const auto filas = repository_.listProducts(pageSize + 1, beforeId);

			const std::size_t count = std::min(
					filas.size(), static_cast<std::size_t>(pageSize)
					);

			bool hayMas = filas.size() > count;
			std::int64_t ultimoIdEntregado = 0;

			// Reserva 64 bytes para el token y su envoltura Protobuf.
			// El mensaje completo queda por debajo de 3 MiB.
			constexpr std::size_t maxResponseBytes = 3U * 1024U * 1024U;
			constexpr std::size_t payloadBudget = maxResponseBytes - 64U;

			ListProductsResponse pagina;

			for (std::size_t i = 0; i < count; ++i) {
				if (context->IsCancelled()) {
					return grpc::Status{
						grpc::StatusCode::CANCELLED,
							"Petición cancelada"
					};
				}

				const auto& source = filas[i];

				// Es el ProductoResumen de Protobuf, en namespace v1.
				ProductoResumen item;
				item.set_product_id(source.product_id);
				item.set_nombre(source.nombre);
				item.set_barcode(source.barcode);
				item.set_precio(source.precio);

				if (!source.thumbnail_key.empty()) {
					try {
						const std::string bytes =
							objectStorage_.getObject(source.thumbnail_key);

						if (!bytes.empty() && bytes.size() <= payloadBudget) {
							item.mutable_miniatura()->set_data(bytes);
						} else {
							std::cerr
								<< "[ListProducts] Miniatura vacía o demasiado "
								<< "grande. Producto: " << source.product_id << '\n';
						}
					} catch (const std::exception& error) {
						// Una miniatura no disponible conserva la tarjeta.
						item.clear_miniatura();
						std::cerr
							<< "[ListProducts] Miniatura no disponible. Producto: "
							<< source.product_id << ": " << error.what() << '\n';
					}
				}

				auto* agregado = pagina.add_productos();
				agregado->Swap(&item);

				if (pagina.ByteSizeLong() > payloadBudget) {
					if (pagina.productos_size() == 1) {
						// Un único producto debe poder avanzar el cursor.
						agregado->clear_miniatura();

						if (pagina.ByteSizeLong() > payloadBudget) {
							return grpc::Status{
								grpc::StatusCode::RESOURCE_EXHAUSTED,
									"Los datos de un producto exceden el límite"
							};
						}
					} else {
						// No se entregó este producto: se recuperará al continuar
						// desde el último ID que sí quedó en la respuesta.
						pagina.mutable_productos()->RemoveLast();
						hayMas = true;
						break;
					}
				}

				ultimoIdEntregado = source.product_id;
			}

			if (hayMas && pagina.productos_size() > 0) {
				pagina.set_next_page_token(std::to_string(ultimoIdEntregado));
			}

			if (context->IsCancelled()) {
				return grpc::Status{
					grpc::StatusCode::CANCELLED,
						"Petición cancelada"
				};
			}

			response->Swap(&pagina);
			return grpc::Status::OK;

		} catch (const std::exception& error) {
			std::cerr << "[ListProducts] Error: " << error.what() << '\n';

			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible obtener los productos"
			};
		}
	}
}

