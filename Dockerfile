FROM archlinux:base-devel

WORKDIR /app

# ---------------------------------------------------------------------------
# Dependencias del sistema
# ---------------------------------------------------------------------------

RUN pacman -Syu --noconfirm \
    cmake \
    ninja \
    git \
    python \
    python-pip \
    libpqxx \
    libvips \
    openssl \
    aws-sdk-cpp \
    pkgconf \
    && pacman -Scc --noconfirm

# ---------------------------------------------------------------------------
# Conan
# ---------------------------------------------------------------------------

RUN python -m venv /opt/conan \
    && /opt/conan/bin/pip install --no-cache-dir conan

ENV PATH="/opt/conan/bin:${PATH}"

# ---------------------------------------------------------------------------
# Dependencias C++ de Conan
#
# IMPORTANTE:
# solo copiamos primero conanfile.py.
#
# Mientras conanfile.py no cambie, esta capa permanece cacheada aunque
# cambien archivos .cpp/.h del backend.
# ---------------------------------------------------------------------------

COPY conanfile.py ./

RUN conan profile detect --force

RUN conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release

# ---------------------------------------------------------------------------
# Código de la aplicación
#
# Los cambios frecuentes ocurren después de haber construido las
# dependencias de Conan.
# ---------------------------------------------------------------------------

COPY . .

RUN conan build . \
    --output-folder=build \
    -s build_type=Release

EXPOSE 50051

CMD ["./build/build/Release/servidor_protobuf_grpc"]

