#pragma once

#include <string>

namespace puntodeventa::storage {

struct S3Config {
    std::string endpoint;
    std::string accessKey;
    std::string secretKey;
    std::string bucket;

    static S3Config fromEnvironment();
};

}
