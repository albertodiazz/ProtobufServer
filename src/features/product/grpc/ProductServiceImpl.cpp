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
#include "features/product/domain/ProductSearchPageToken.h"
#include "features/product/application/IdempotentTools.h"

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
		target->set_cantidad(source.cantidad);
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

		std::cout
			<< "[1] Inicio CreateProduct\n";


		/*
		 * =====================================================
		 * 1. Validar UUID
		 * =====================================================
		 */

		const std::string& uuid =
			request->uuid();


		if (!puntodeventa::product::isValidUuid(uuid)) {

			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					"UUID invalido"
			};
		}


		/*
		 * Cancelación antes de empezar cualquier
		 * operación persistente.
		 */
		if (context->IsCancelled()) {

			return grpc::Status{
				grpc::StatusCode::CANCELLED,
					"Peticion cancelada"
			};
		}


		/*
		 * =====================================================
		 * 2. Validar datos del producto
		 * =====================================================
		 */

		std::string extension;
		std::string contentType;


		const std::string& imageData =
			request->imagen().data();


		/*
		 * Todavía no conocemos el barcode definitivo.
		 *
		 * Primero validamos los demás datos.
		 */

		puntodeventa::product::Producto producto{
			.nombre =
				request->nombre(),

				.barcode =
					"",

				.descripcion =
					request->descripcion(),

				.precio =
					request->precio(),

				.costo =
					request->costo(),

				.image_key =
					imageData,

				.cantidad =
					request->cantidad()
		};


		const auto validation =
			ProductValidator::validate(
					producto.nombre,
					producto.descripcion,
					producto.precio,
					producto.costo,
					producto.image_key,

					puntodeventa::image::detectImageFormat(
						producto.image_key
						),

					extension,
					contentType,
					producto.cantidad
					);


		if (validation) {

			const std::string message =
				ProductValidator::
				validationErrorMessage(
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
		 * =====================================================
		 * 3. Hash de la petición
		 * =====================================================
		 */

		std::string requestHash;

		try {

			requestHash =
				puntodeventa::product::buildCreateProductRequestHash(
						*request
						);

		}
		catch (const std::exception& e) {

			std::cerr
				<< "[CreateProduct] Error hash: "
				<< e.what()
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible procesar la peticion"
			};
		}


		/*
		 * =====================================================
		 * 4. Generar barcode candidato
		 * =====================================================
		 *
		 * Este solamente se utilizará si este UUID
		 * nunca había sido registrado.
		 */

		const std::string proposedBarcode =
			puntodeventa::generadorBarcode();


		/*
		 * =====================================================
		 * 5. Reservar UUID
		 * =====================================================
		 */

		puntodeventa::product::CreateProductReservation
			reservation;


		try {

			reservation =
				repository_
				.reserveCreateProductRequest(
						uuid,
						requestHash,
						proposedBarcode
						);

		}
		catch (const std::exception& e) {

			std::cerr
				<< "[CreateProduct] Error reservando UUID: "
				<< e.what()
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible reservar la operacion"
			};
		}


		/*
		 * =====================================================
		 * 6. Mismo UUID pero petición diferente
		 * =====================================================
		 */

		if (
				reservation.state ==
				puntodeventa::product::
				CreateProductReservationState::Conflict
			 ) {

			std::cerr
				<< "[CreateProduct] UUID reutilizado "
				<< "con datos diferentes: "
				<< uuid
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::FAILED_PRECONDITION,

					"El UUID ya fue utilizado "
						"para otra operacion"
			};
		}


		/*
		 * =====================================================
		 * 7. Operación ya completada
		 * =====================================================
		 *
		 * No generamos imagen.
		 * No escribimos S3.
		 * No hacemos INSERT.
		 *
		 * Simplemente devolvemos la respuesta anterior.
		 */

		if (
				reservation.state ==
				puntodeventa::product::
				CreateProductReservationState::Completed
			 ) {

			if (!reservation.product_id) {

				std::cerr
					<< "[CreateProduct] COMPLETED "
					<< "sin product_id\n";


				return grpc::Status{
					grpc::StatusCode::INTERNAL,
						"Estado de idempotencia inconsistente"
				};
			}


			std::cout
				<< "[CreateProduct] Retry COMPLETED. UUID: "
				<< uuid
				<< '\n';


			response->set_ok(
					true
					);

			response->set_mensaje(
					"El producto fue guardado correctamente"
					);

			response->set_product_id(
					*reservation.product_id
					);

			response->set_internal_barcode(
					reservation.barcode
					);


			return grpc::Status::OK;
		}


		/*
		 * =====================================================
		 * 8. ACQUIRED o PROCESSING
		 * =====================================================
		 *
		 * En AMBOS casos utilizamos el barcode
		 * almacenado en PostgreSQL.
		 *
		 * Nunca volvemos a usar proposedBarcode.
		 */

		producto.barcode =
			reservation.barcode;


		std::cout
			<< "[CreateProduct] UUID: "
			<< uuid
			<< '\n';

		std::cout
			<< "[CreateProduct] Barcode reservado: "
			<< producto.barcode
			<< '\n';


		if (
				reservation.state ==
				puntodeventa::product::
				CreateProductReservationState::Processing
			 ) {

			std::cout
				<< "[CreateProduct] "
				<< "Reanudando operación PROCESSING\n";
		}


		/*
		 * =====================================================
		 * 9. Thumbnail
		 * =====================================================
		 */

		ThumbnailGenerator thumbnailGenerator;


		Thumbnail thumbnail;

		try {

			thumbnail =
				thumbnailGenerator.generate(
						imageData
						);

		}
		catch (const std::exception& e) {

			std::cerr
				<< "[CreateProduct] Thumbnail error: "
				<< e.what()
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible generar la miniatura"
			};
		}


		/*
		 * =====================================================
		 * 10. S3
		 * =====================================================
		 *
		 * Como usamos SIEMPRE el mismo barcode:
		 *
		 * products/<barcode>/main...
		 * products/<barcode>/thumb...
		 *
		 * un retry vuelve a escribir las mismas keys.
		 */

		std::string imageKey;
		std::string imageKeyThumbnail;


		try {

			std::cout
				<< "[6] Antes de S3\n";


			imageKey =
				objectStorage_.putObject(
						"products/" +
						producto.barcode +
						"/main" +
						extension,

						imageData,

						contentType
						);


			imageKeyThumbnail =
				objectStorage_.putObject(
						"products/" +
						producto.barcode +
						"/thumb" +
						thumbnail.extension,

						thumbnail.data,

						contentType
						);


			std::cout
				<< "[7] S3 terminado. Key: "
				<< imageKey
				<< '\n';

		}
		catch (const std::exception& e) {

			std::cerr
				<< "[CreateProduct] Error S3: "
				<< e.what()
				<< '\n';


			/*
			 * Dejamos la operación PROCESSING.
			 *
			 * Un retry con el mismo UUID y mismo hash
			 * obtendrá el mismo barcode y volverá
			 * a intentar exactamente estas mismas keys.
			 */
			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible almacenar la imagen"
			};
		}


		/*
		 * Las referencias que guardaremos en PostgreSQL.
		 */
		producto.image_key =
			imageKey;

		producto.thumbnail_key =
			imageKeyThumbnail;


		/*
		 * =====================================================
		 * 11. Crear producto + completar UUID
		 * =====================================================
		 *
		 * createIdempotent():
		 *
		 * BEGIN
		 *
		 * SELECT request FOR UPDATE
		 *
		 * INSERT products
		 *
		 * UPDATE create_product_requests
		 *      status = COMPLETED
		 *      product_id = ...
		 *
		 * COMMIT
		 */

		std::int64_t productoId;


		try {

			productoId =
				repository_.createIdempotent(
						producto,
						uuid
						);

		}
		catch (const std::exception& e) {

			std::cerr
				<< "[CreateProduct] "
				<< "Error createIdempotent: "
				<< e.what()
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible guardar el producto"
			};
		}


		std::cout
			<< "[9] Producto guardado ID: "
			<< productoId
			<< '\n';


		/*
		 * =====================================================
		 * 12. Respuesta
		 * =====================================================
		 *
		 * A partir de aquí el COMMIT ya ocurrió.
		 *
		 * NO comprobamos context->IsCancelled().
		 *
		 * Aunque Android haya perdido conexión,
		 * el producto YA existe.
		 *
		 * Android deberá reintentar usando
		 * exactamente el mismo UUID.
		 */

		response->set_ok(
				true
				);

		response->set_mensaje(
				"El producto fue guardado correctamente"
				);

		response->set_product_id(
				productoId
				);

		response->set_internal_barcode(
				producto.barcode
				);


		std::cout
			<< "[10] RPC terminado\n";


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
					contentType,
					productoRequest.cantidad()
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
				.thumbnail_key = imageKey_thumbnail,
				.cantidad = productoRequest.cantidad() 
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

		/*
		 * 1. Comprobar cancelación antes de operar.
		 */
		if (context->IsCancelled()) {

			return grpc::Status{
				grpc::StatusCode::CANCELLED,
				"Petición cancelada"
			};
		}


		/*
		 * 2. Obtener barcode.
		 */
		const std::string barcode =
			request->barcode();


		/*
		 * 3. Validar barcode con la misma regla
		 *    que ya utilizas en los otros endpoints.
		 */
		const auto barcodeValidation =
			ProductValidator::validateBarCode(
					barcode
					);


		if (barcodeValidation) {

			const std::string message =
				ProductValidator::validationErrorMessage(
						*barcodeValidation
						);

			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					message
			};
		}


		/*
		 * 4. Eliminar producto.
		 */
		try {

			const bool deleted =
				repository_.deleteByBarcode(
						barcode
						);


			if (!deleted) {

				return grpc::Status{
					grpc::StatusCode::NOT_FOUND,
						"Producto no encontrado"
				};
			}


			response->set_ok(true);

			return grpc::Status::OK;

		} catch (const std::exception& e) {

			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					e.what()
			};
		}
	}

	grpc::Status ProductServiceImpl::ListProducts(
			grpc::ServerContext* context,
			const ListProductsRequest* request,
			ListProductsResponse* response
			) {

		/*
		 * 1. Validar page_size.
		 */
		if (request->page_size() < 0) {

			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
				"page_size no puede ser negativo"
			};
		}


		/*
		 * page_size = 0:
		 * usar valor por defecto.
		 *
		 * Máximo permitido:
		 * 100 productos.
		 */
		const std::int32_t pageSize =
			request->page_size() == 0
			? 20
			: std::min<std::int32_t>(
					request->page_size(),
					100
					);


		/*
		 * 2. Decodificar cursor.
		 *
		 * Primera página:
		 * page_token = ""
		 *
		 * Siguientes páginas:
		 * page_token = último product_id entregado.
		 */
		std::optional<std::int64_t> beforeId;

		const std::string& token =
			request->page_token();


		if (!token.empty()) {

			/*
			 * int64 positivo:
			 * máximo práctico 19 dígitos.
			 */
			if (token.size() > 19) {

				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"page_token inválido"
				};
			}


			std::int64_t id = 0;

			const auto parsed =
				std::from_chars(
						token.data(),
						token.data() + token.size(),
						id
						);


			if (
					parsed.ec != std::errc{} ||
					parsed.ptr != token.data() + token.size() ||
					id <= 0
				 ) {

				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"page_token inválido"
				};
			}


			beforeId = id;
		}


		/*
		 * 3. Comprobar cancelación antes
		 * de acceder a infraestructura.
		 */
		if (context->IsCancelled()) {

			return grpc::Status{
				grpc::StatusCode::CANCELLED,
					"Petición cancelada"
			};
		}


		try {

			/*
			 * Pedimos una fila adicional para saber
			 * si existe una siguiente página.
			 *
			 * pageSize = 20
			 * repository = 21
			 */
			const auto filas =
				repository_.listProducts(
						pageSize + 1,
						beforeId
						);


			/*
			 * Nunca entregamos más de pageSize.
			 */
			const std::size_t count =
				std::min(
						filas.size(),
						static_cast<std::size_t>(
							pageSize
							)
						);


			/*
			 * Si repository devolvió una fila adicional,
			 * sabemos que existen más productos.
			 */
			bool hayMas =
				filas.size() > count;


			std::int64_t ultimoIdEntregado = 0;


			/*
			 * Dejamos margen para:
			 *
			 * - next_page_token
			 * - metadata protobuf
			 * - envoltura gRPC
			 */
			constexpr std::size_t maxResponseBytes =
				3U * 1024U * 1024U;

			constexpr std::size_t payloadBudget =
				maxResponseBytes - 64U;


			ListProductsResponse pagina;


			/*
			 * 4. Construir página.
			 */
			for (
					std::size_t i = 0;
					i < count;
					++i
					) {

				if (context->IsCancelled()) {

					return grpc::Status{
						grpc::StatusCode::CANCELLED,
							"Petición cancelada"
					};
				}


				const auto& source =
					filas[i];


				/*
				 * Agregamos directamente el elemento
				 * protobuf a la página.
				 */
				auto* agregado =
					pagina.add_productos();


				/*
				 * Llenado común:
				 *
				 * id
				 * nombre
				 * barcode
				 * precio
				 * cantidad
				 * miniatura
				 */
				fillProductoResumen(
						source,
						agregado
						);


				/*
				 * 5. Verificar tamaño acumulado.
				 */
				if (
						pagina.ByteSizeLong() >
						payloadBudget
					 ) {

					/*
					 * Si es el único producto de la página,
					 * intentamos conservarlo quitando
					 * únicamente la miniatura.
					 */
					if (
							pagina.productos_size() == 1
						 ) {

						agregado->clear_miniatura();


						if (
								pagina.ByteSizeLong() >
								payloadBudget
							 ) {

							return grpc::Status{
								grpc::StatusCode::RESOURCE_EXHAUSTED,
									"Los datos de un producto exceden el límite"
							};
						}

					} else {

						/*
						 * Este producto todavía NO fue
						 * entregado.
						 *
						 * Lo eliminamos y será recuperado
						 * en la siguiente página utilizando
						 * ultimoIdEntregado.
						 */
						pagina
							.mutable_productos()
							->RemoveLast();


						hayMas = true;

						break;
					}
				}


				/*
				 * Solo actualizamos el cursor después
				 * de confirmar que el producto quedó
				 * realmente dentro de la respuesta.
				 */
				ultimoIdEntregado =
					source.product_id;
			}


			/*
			 * 6. Construir cursor siguiente.
			 */
			if (
					hayMas &&
					pagina.productos_size() > 0
				 ) {

				pagina.set_next_page_token(
						std::to_string(
							ultimoIdEntregado
							)
						);
			}


			/*
			 * 7. Última oportunidad de abortar antes
			 * de entregar la respuesta.
			 */
			if (context->IsCancelled()) {

				return grpc::Status{
					grpc::StatusCode::CANCELLED,
						"Petición cancelada"
				};
			}


			response->Swap(
					&pagina
					);


			return grpc::Status::OK;


		} catch (const std::exception& error) {

			std::cerr
				<< "[ListProducts] Error: "
				<< error.what()
				<< '\n';


			return grpc::Status{
				grpc::StatusCode::INTERNAL,
					"No fue posible obtener los productos"
			};
		}
	}

	grpc::Status ProductServiceImpl::SearchProducts(
			grpc::ServerContext* context,
			const SearchProductsRequest* request,
			SearchProductsResponse* response
			) {

		/*
		 * 1. Cancelación.
		 */
		if (context->IsCancelled()) {
			return grpc::Status{
				grpc::StatusCode::CANCELLED,
				"Peticion cancelada"
			};
		}


		/*
		 * 2. Validar query.
		 */
		if (request->query().empty()) {
			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
				"query no puede estar vacio"
			};
		}


		/*
		 * 3. Validar page size.
		 */
		if (
				request->page_size() < 1 ||
				request->page_size() > 100
			 ) {
			return grpc::Status{
				grpc::StatusCode::INVALID_ARGUMENT,
					"page_size debe estar entre 1 y 100"
			};
		}


		/*
		 * 4. Decodificar cursor.
		 */
		std::optional<
			puntodeventa::product::ProductSearchCursor
			> cursor;


		if (!request->page_token().empty()) {

			cursor =
				puntodeventa::product::
				decodeSearchPageToken(
						request->page_token()
						);


			if (!cursor) {
				return grpc::Status{
					grpc::StatusCode::INVALID_ARGUMENT,
						"page_token invalido"
				};
			}
		}


		/*
		 * 5. Buscar.
		 */
		const auto page =
			repository_.searchProducts(
					request->query(),
					request->page_size(),
					cursor
					);


		/*
		 * 6. Construir productos.
		 */
		for (const auto& producto : page.productos) {

			auto* protoProducto =
				response->add_productos();

			fillProductoResumen(
					producto,
					protoProducto
					);

			/*
			 * Aquí todavía falta resolver thumbnail
			 * de la misma manera que ya lo haces en
			 * ListProducts.
			 */
		}


		/*
		 * 7. Crear token para la siguiente página.
		 */
		if (page.next_cursor) {

			response->set_next_page_token(
					puntodeventa::product::
					encodeSearchPageToken(
						*page.next_cursor
						)
					);
		}


		return grpc::Status::OK;
	}

	void ProductServiceImpl::fillProductoResumen(
			const puntodeventa::product::ProductoResumen& producto,
			::puntodeventa::v1::ProductoResumen* protoProducto
			) {

		protoProducto->set_product_id(
				producto.product_id
				);

		protoProducto->set_nombre(
				producto.nombre
				);

		protoProducto->set_barcode(
				producto.barcode
				);

		protoProducto->set_precio(
				producto.precio
				);

		protoProducto->set_cantidad(
				producto.cantidad
				);


		/*
		 * Si no existe thumbnail, simplemente
		 * dejamos miniatura vacía.
		 */
		if (producto.thumbnail_key.empty()) {
			return;
		}


		try {

			/*
			 * getObject devuelve los bytes crudos
			 * almacenados en RustFS / S3.
			 */
			const std::string thumbnail =
				objectStorage_.getObject(
						producto.thumbnail_key
						);


			if (thumbnail.empty()) {
				return;
			}


			auto* miniatura =
				protoProducto->mutable_miniatura();

			miniatura->set_data(
					thumbnail
					);

		} catch (const std::exception& e) {

			/*
			 * Fallar al descargar una miniatura
			 * no debería impedir devolver el producto.
			 */
			std::cerr
				<< "[THUMBNAIL-ERROR] product_id="
				<< producto.product_id
				<< " key="
				<< producto.thumbnail_key
				<< " error="
				<< e.what()
				<< '\n';
		}
	}


}

