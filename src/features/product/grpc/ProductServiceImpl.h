#pragma once

#include "punto_de_venta.grpc.pb.h"
#include "features/product/domain/ProductRepository.h"
#include "features/product/application/CreateProduct.h"


#ifdef _WIN32
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT __declspec(dllexport)
#else
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT
#endif

namespace puntodeventa::storage {
    class ObjectStorage;
}

namespace puntodeventa::v1{

	class SERVIDOR_PROTOBUF_GRPC_EXPORT ProductServiceImpl final 
		: public ProductService::Service {

			public:

				explicit ProductServiceImpl(
						::puntodeventa::product::ProductRepository& respository,
						::puntodeventa::storage::ObjectStorage& objectStorage
						);

				grpc::Status CreateProduct(
						grpc::ServerContext* context,
						const CreateProductRequest* request,
						CreateProductResponse* response
						) override;

				grpc::Status GetProductById(
						grpc::ServerContext* context,
						const GetProductByIdRequest* request,
						GetProductResponse *response
						) override; 

				grpc::Status GetProductByBarcode(
						grpc::ServerContext* context,
						const GetProductByBarcodeRequest* request,
						GetProductResponse *response
						) override; 

				grpc::Status UpdateProduct(
						grpc::ServerContext* context,
						const UpdateProductRequest* request,
						UpdateProductResponse* response
						) override; 

				grpc::Status DeleteProduct(
						grpc::ServerContext* context,
						const DeleteProductRequest* request,
						DeleteProductResponse* response
						) override; 


			private:
				puntodeventa::product::ProductRepository& repository_;
				puntodeventa::storage::ObjectStorage& objectStorage_;
				::puntodeventa::product::CreateProduct& createProduct_;

		};
}
