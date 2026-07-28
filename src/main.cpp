#include "servidor_protobuf_grpc.h"
#include <vector>
#include <string>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "punto_de_venta.pb.h"

namespace puntodeventa::v1{ 

		grpc::Status RouteServerImpl::Ping(	
				grpc::ServerContext* context,
				const PingRequest* request,
				PingResponse* response
				){

			const std::string& cliente = request->cliente();

			std::cout << "Ping del cliente: "<< cliente << std::endl;

			// De que todo bien con el mensaje recibido
			response->set_conectado(true);
			response->set_mensaje("Conexion exitosa con el cliente: " + cliente);

			return grpc::Status::OK;
		}
}

int main(int argc, char* argv[]) {
	
		GOOGLE_PROTOBUF_VERIFY_VERSION;

		std::string server_addres("0.0.0.0:50051");
		puntodeventa::v1::RouteServerImpl servicio;
	
		grpc::reflection::InitProtoReflectionServerBuilderPlugin();
		grpc::ServerBuilder builder;
		builder.AddListeningPort(server_addres,grpc::InsecureServerCredentials());
		builder.RegisterService(&servicio);

		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		std::cout << "Server listening on: " << server_addres << std::endl;
		server->Wait();
}
