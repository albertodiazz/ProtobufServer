#include "DataBaseConnection.h"

namespace puntodeventa::database {

DatabaseConnection::DatabaseConnection(
    const DatabaseConfig& config
)
    : connection_(
        "host=" + config.host +
        " port=" + config.port +
        " dbname=" + config.dbname +
        " user=" + config.user +
        " password=" + config.password
    )
{
}

pqxx::connection& DatabaseConnection::get()
{
    return connection_;
}

bool DatabaseConnection::isOpen() const
{
    return connection_.is_open();
}

}
