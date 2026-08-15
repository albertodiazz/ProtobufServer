// DatabaseConfig.cpp

#include "DataBaseConfig.h"

#include <cstdlib>
#include <stdexcept>

namespace puntodeventa::database {

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

DatabaseConfig DatabaseConfig::fromEnvironment()
{
    return DatabaseConfig{
        .host = getEnv("DB_HOST"),
        .port = getEnv("DB_PORT"),
        .dbname = getEnv("DB_NAME"),
        .user = getEnv("DB_USER"),
        .password = getEnv("DB_PASSWORD")
    };
}

}
