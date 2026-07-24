cmake_minimum_required(VERSION 3.25)

project(
    servidor_protobuf_grpc
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Conan genera protobuf-config.cmake y gRPCConfig.cmake.
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Forzar las herramientas contenidas en los paquetes de Conan.
#
# Evita que los helpers de ConanCenter seleccionen:
#   /usr/bin/protoc
#   /usr/bin/grpc_cpp_plugin
# ---------------------------------------------------------------------------

set(CONAN_PROTOC
    "${protobuf_PACKAGE_FOLDER_RELEASE}/bin/protoc"
)

set(CONAN_GRPC_CPP_PLUGIN
    "${gRPC_PACKAGE_FOLDER_RELEASE}/bin/grpc_cpp_plugin"
)

if(NOT EXISTS "${CONAN_PROTOC}")
    message(FATAL_ERROR
        "No se encontró protoc de Conan en: ${CONAN_PROTOC}"
    )
endif()

if(NOT EXISTS "${CONAN_GRPC_CPP_PLUGIN}")
    message(FATAL_ERROR
        "No se encontró grpc_cpp_plugin de Conan en: "
        "${CONAN_GRPC_CPP_PLUGIN}"
    )
endif()

# Corrige las variables usadas por los módulos de Protobuf.
set(
    Protobuf_PROTOC_EXECUTABLE
    "${CONAN_PROTOC}"
    CACHE FILEPATH
    "protoc proporcionado por Conan"
    FORCE
)

set(
    PROTOC_PROGRAM
    "${CONAN_PROTOC}"
    CACHE FILEPATH
    "protoc proporcionado por Conan"
    FORCE
)

set(
    GRPC_CPP_PLUGIN_PROGRAM
    "${CONAN_GRPC_CPP_PLUGIN}"
    CACHE FILEPATH
    "grpc_cpp_plugin proporcionado por Conan"
    FORCE
)

# Corrige también los targets importados.
set_target_properties(
    protobuf::protoc
    PROPERTIES
        IMPORTED_LOCATION "${CONAN_PROTOC}"
        IMPORTED_LOCATION_RELEASE "${CONAN_PROTOC}"
)

set_target_properties(
    gRPC::grpc_cpp_plugin
    PROPERTIES
        IMPORTED_LOCATION "${CONAN_GRPC_CPP_PLUGIN}"
        IMPORTED_LOCATION_RELEASE "${CONAN_GRPC_CPP_PLUGIN}"
)

message(STATUS "protoc seleccionado: ${CONAN_PROTOC}")
message(
    STATUS
    "grpc_cpp_plugin seleccionado: ${CONAN_GRPC_CPP_PLUGIN}"
)

# ---------------------------------------------------------------------------
# Archivos Protocol Buffers
# ---------------------------------------------------------------------------

set(PROTO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

set(PROTO_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set(PROTO_FILES
    "${PROTO_ROOT}/punto_de_venta.proto"
)

file(MAKE_DIRECTORY
    "${PROTO_GENERATED_DIR}"
)

# Biblioteca interna que contendrá todo el código generado.
add_library(pos_proto STATIC
    ${PROTO_FILES}
)

target_link_libraries(pos_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

target_include_directories(pos_proto
    PUBLIC
        "${PROTO_GENERATED_DIR}"
)

# Genera:
#   punto_de_venta.pb.cc
#   punto_de_venta.pb.h
protobuf_generate(
    TARGET pos_proto
    LANGUAGE cpp
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS "${PROTO_ROOT}"
    PROTOC_OUT_DIR "${PROTO_GENERATED_DIR}"
)

# Genera:
#   punto_de_venta.grpc.pb.cc
#   punto_de_venta.grpc.pb.h
protobuf_generate(
    TARGET pos_proto
    LANGUAGE grpc
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS "${PROTO_ROOT}"
    PROTOC_OUT_DIR "${PROTO_GENERATED_DIR}"
    GENERATE_EXTENSIONS
        .grpc.pb.h
        .grpc.pb.cc
    PLUGIN
        "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

# ---------------------------------------------------------------------------
# Servidor
# ---------------------------------------------------------------------------

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/servidor_protobuf_grpc.cpp
    src/servidor_protobuf_grpc.h
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        pos_proto
)

# ---------------------------------------------------------------------------
# Instalación
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)


