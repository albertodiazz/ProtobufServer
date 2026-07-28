#pragma once

#include <vector>
#include <string>
#include "punto_de_venta.grpc.pb.h"


#ifdef _WIN32
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT __declspec(dllexport)
#else
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT
#endif

SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc();
SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc_print_vector(const std::vector<std::string> &strings);

SERVIDOR_PROTOBUF_GRPC_EXPORT void server();

namespace puntodeventa::v1{

	class SERVIDOR_PROTOBUF_GRPC_EXPORT RouteServerImpl final 
		: public ConexionService::Service {

			public:

				grpc::Status Ping(
						grpc::ServerContext* context,
						const PingRequest* request,
						PingResponse* response
						) override;
						
		};
}
