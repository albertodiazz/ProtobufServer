#pragma once

#include <pqxx/pqxx>
#include <string>
#include "DataBaseConfig.h"

namespace puntodeventa::database {

class DatabaseConnection : DatabaseConfig{
public:
    explicit DatabaseConnection(
        const DatabaseConfig& config
    );

    pqxx::connection& get();

    bool isOpen() const;

private:
    pqxx::connection connection_;
};

}
