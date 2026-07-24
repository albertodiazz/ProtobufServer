from conan import ConanFile
from conan.tools.cmake import (
    CMake,
    CMakeDeps,
    CMakeToolchain,
    cmake_layout,
)


class ServidorProtobufGrpcRecipe(ConanFile):
    name = "servidor_protobuf_grpc"
    version = "0.1.0"
    package_type = "application"

    license = "Proprietary"
    author = "Alberto Díaz"
    description = "Backend gRPC del punto de venta"
    topics = ("grpc", "protobuf", "backend", "pos")

    settings = (
        "os",
        "compiler",
        "build_type",
        "arch",
    )

    # Incluye todo lo necesario para reconstruir el proyecto.
    exports_sources = (
        "CMakeLists.txt",
        "src/*",
        "include/*",
        "proto/*",
    )

    # Versiones explícitas para tener builds reproducibles.
    def requirements(self):
        self.requires("grpc/1.82.0")
        self.requires("protobuf/6.33.5")

    # Configuración de las dependencias.
    default_options = {
        # Bibliotecas estáticas.
        "grpc/*:shared": False,
        "protobuf/*:shared": False,

        # Necesarios para generar código C++ desde archivos .proto.
        "grpc/*:codegen": True,
        "grpc/*:cpp_plugin": True,

        # Plugins de otros lenguajes que tu backend no necesita.
        "grpc/*:csharp_plugin": False,
        "grpc/*:node_plugin": False,
        "grpc/*:objective_c_plugin": False,
        "grpc/*:php_plugin": False,
        "grpc/*:python_plugin": False,
        "grpc/*:ruby_plugin": False,

        # Tu backend usa la implementación completa de Protobuf.
        "protobuf/*:lite": False,
        "protobuf/*:with_zlib": True,
    }

    def layout(self):
        cmake_layout(self)

    def generate(self):
        dependencies = CMakeDeps(self)
        dependencies.generate()

        toolchain = CMakeToolchain(self)
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
