#pragma once

#include <optional>
#include <string>

#include "features/product/domain/Product.h"

namespace puntodeventa::product {

std::string encodeSearchPageToken(
    const ProductSearchCursor& cursor
);

std::optional<ProductSearchCursor>
decodeSearchPageToken(
    const std::string& token
);

}
