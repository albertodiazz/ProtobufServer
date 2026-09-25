#pragma once

#include <string>

#include "punto_de_venta.pb.h"

namespace puntodeventa::product {

bool isValidUuid(
        const std::string& uuid
        );


std::string buildCreateProductRequestHash(
        const ::puntodeventa::v1::CreateProductRequest& request
        );

}
