FROM archlinux:base-devel

WORKDIR /app

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

RUN python -m venv /opt/conan \
    && /opt/conan/bin/pip install --no-cache-dir conan

ENV PATH="/opt/conan/bin:${PATH}"

COPY . .

RUN conan profile detect --force

RUN conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release

RUN conan build . \
    --output-folder=build \
    -s build_type=Release

EXPOSE 50051

CMD ["./build/build/Release/servidor_protobuf_grpc"]
