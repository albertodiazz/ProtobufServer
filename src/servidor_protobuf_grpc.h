#pragma once

#include <vector>
#include <string>
// #include "core/database/S3ObjectStorage/OjectStorage.h"
// #include "features/product/domain/ProductRepository.h"
// #include "punto_de_venta.grpc.pb.h"


#ifdef _WIN32
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT __declspec(dllexport)
#else
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT
#endif

SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc();
SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc_print_vector(const std::vector<std::string> &strings);

SERVIDOR_PROTOBUF_GRPC_EXPORT void server();
