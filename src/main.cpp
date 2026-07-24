#include "servidor_protobuf_grpc.h"
#include <vector>
#include <string>

int main() {
    servidor_protobuf_grpc();

    std::vector<std::string> vec;
    vec.push_back("test_package");

    servidor_protobuf_grpc_print_vector(vec);
}
