#include "core/database/DataBaseConfig.h"
#include "core/database/DataBaseConnection.h"
#include "core/database/S3ObjectStorage/S3Config.h"
#include "core/database/S3ObjectStorage/S3ObjectStorage.h"
#include "features/product/data/SqlProductRepository.h"
#include "servidor_protobuf_grpc.h"
#include <vector>
#include <string>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <aws/core/Aws.h>

#include "punto_de_venta.pb.h"
#include "features/BarcodeGenerator.h"
#include "features/product/domain/Product.h"
#include "core/image/ImageFormatDetector.h"

namespace puntodeventa::v1{ 

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

}

int main(int argc, char* argv[]) {

	GOOGLE_PROTOBUF_VERIFY_VERSION;
	Aws::SDKOptions options;
	Aws::InitAPI(options);

	auto dbConfig =
		puntodeventa::database::DatabaseConfig::fromEnvironment();

	auto s3Config =
		puntodeventa::storage::S3Config::fromEnvironment();

	///////////////////////////////////
	// Configuracion de S3 y database
	///////////////////////////////////
	puntodeventa::storage::S3ObjectStorage objectStorage{
		s3Config
	};

	puntodeventa::database::DatabaseConnection database{
		dbConfig
	};
	///////////////////////////////////
	//Implementacion 
	///////////////////////////////////
	puntodeventa::product::SqlProductRepository productRepository{
		database.get()
	};

	puntodeventa::v1::ProductServiceImpl servicioProducto{
		productRepository,
			objectStorage
	};


	std::string server_addres("0.0.0.0:50051");

	grpc::reflection::InitProtoReflectionServerBuilderPlugin();
	grpc::ServerBuilder builder;

	builder.AddListeningPort(
			server_addres,
			grpc::InsecureServerCredentials()
			);
	builder.RegisterService(&servicioProducto);

	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	std::cout << "Server listening on: " << server_addres << std::endl;
	server->Wait();

	Aws::ShutdownAPI(options);

	return 0;
}
