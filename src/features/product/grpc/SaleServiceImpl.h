#pragma once

#include "punto_de_venta.grpc.pb.h"
#include "features/product/domain/SaleRepository.h"

#ifdef _WIN32
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT __declspec(dllexport)
#else
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT
#endif

namespace puntodeventa::v1 {

    class SERVIDOR_PROTOBUF_GRPC_EXPORT SaleServiceImpl final
        : public SaleService::Service {

        public:

            explicit SaleServiceImpl(
                    ::puntodeventa::sale::SaleRepository& repository
                    );

            grpc::Status CreateSale(
                    grpc::ServerContext* context,
                    const CreateSaleRequest* request,
                    CreateSaleResponse* response
                    ) override;

        private:

            ::puntodeventa::sale::SaleRepository& repository_;
    };

}
