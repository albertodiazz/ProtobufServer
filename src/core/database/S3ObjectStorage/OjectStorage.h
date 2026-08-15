#pragma once

#include <string>
#include <string_view>

namespace puntodeventa::storage {

class ObjectStorage {
public:
    virtual ~ObjectStorage() = default;

    virtual std::string putObject(
        const std::string& key,
        std::string_view data,
        const std::string& contentType
    ) = 0;
};

}
