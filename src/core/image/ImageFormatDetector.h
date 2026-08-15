#pragma once

#include "core/image/ImageFormat.h"

#include <string_view>

namespace puntodeventa::image {

ImageFormat detectImageFormat(
    std::string_view data
);

}
