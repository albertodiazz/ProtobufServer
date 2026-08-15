#include "core/database/S3ObjectStorage/S3Config.h"

#include <cstdlib>
#include <stdexcept>

namespace puntodeventa::storage {

static std::string getEnv(const char* name)
{
    const char* value = std::getenv(name);

    if (value == nullptr) {
        throw std::runtime_error(
            std::string{"Variable de entorno faltante: "} + name
        );
    }

    return value;
}

S3Config S3Config::fromEnvironment()
{
    return S3Config{
        .endpoint = getEnv("S3_ENDPOINT"),
        .accessKey = getEnv("S3_ACCESS_KEY"),
        .secretKey = getEnv("S3_SECRET_KEY"),
        .bucket = getEnv("S3_BUCKET")
    };
}

}
