cmake_minimum_required(VERSION 3.25)

project(
    servidor_protobuf_grpc
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libpqxx REQUIRED)

# Dependencias generadas/resueltas por Conan.
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

if(NOT TARGET protobuf::protoc)
    message(
        FATAL_ERROR
        "Conan no proporcionó el target protobuf::protoc"
    )
endif()

if(NOT TARGET gRPC::grpc_cpp_plugin)
    message(
        FATAL_ERROR
        "Conan no proporcionó el target gRPC::grpc_cpp_plugin"
    )
endif()

pkg_check_modules(
    VIPS
    REQUIRED
    IMPORTED_TARGET
    "vips-cpp>=8.16"
)

# ---------------------------------------------------------------------------
# AWS SDK
# ---------------------------------------------------------------------------

if(DEFINED ENV{AWS_SDK_ROOT})
    list(
        PREPEND
        CMAKE_PREFIX_PATH
        "$ENV{AWS_SDK_ROOT}"
    )

    message(
        STATUS
        "AWS SDK root: $ENV{AWS_SDK_ROOT}"
    )
endif()

find_package(
    AWSSDK
    REQUIRED
    COMPONENTS s3
)

# ---------------------------------------------------------------------------
# Archivos Protocol Buffers
# ---------------------------------------------------------------------------

set(
    PROTO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

set(
    PROTO_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set(
    PROTO_FILES
    "${PROTO_ROOT}/punto_de_venta.proto"
)

file(
    MAKE_DIRECTORY
    "${PROTO_GENERATED_DIR}"
)

add_library(
    pos_proto
    STATIC
    ${PROTO_FILES}
)

target_link_libraries(
    pos_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
)

target_include_directories(
    pos_proto
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

file(
    GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(
    ${PROJECT_NAME}
    ${PROJECT_SOURCES}
    src/servidor_protobuf_grpc.h
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        pos_proto
        libpqxx::pqxx
        ${AWSSDK_LINK_LIBRARIES}
        PkgConfig::VIPS
        OpenSSL::Crypto
)

# ---------------------------------------------------------------------------
# Instalación
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
