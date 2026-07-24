#pragma once

#include <vector>
#include <string>


#ifdef _WIN32
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT __declspec(dllexport)
#else
  #define SERVIDOR_PROTOBUF_GRPC_EXPORT
#endif

SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc();
SERVIDOR_PROTOBUF_GRPC_EXPORT void servidor_protobuf_grpc_print_vector(const std::vector<std::string> &strings);
