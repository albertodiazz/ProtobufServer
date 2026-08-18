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


#include "features/product/grpc/ProductServiceImpl.h"


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
