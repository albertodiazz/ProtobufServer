// DatabaseConfig.h
#pragma once

#include <string>

namespace puntodeventa::database {

	struct DatabaseConfig {
		std::string host;
		std::string port;
		std::string dbname;
		std::string user;
		std::string password;

		static DatabaseConfig fromEnvironment();
	}; 

}
