#include "ProductSearchPageToken.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace puntodeventa::product {

std::string encodeSearchPageToken(
    const ProductSearchCursor& cursor
) {
    std::ostringstream stream;

    stream
        << "v1"
        << '|'
        << (cursor.exact_match ? 1 : 0)
        << '|'
        << std::setprecision(
            std::numeric_limits<float>::max_digits10
        )
        << cursor.rank
        << '|'
        << cursor.product_id;

    return stream.str();
}


std::optional<ProductSearchCursor>
decodeSearchPageToken(
    const std::string& token
) {

    if (token.empty()) {
        return std::nullopt;
    }

    std::istringstream stream{token};

    std::string version;
    std::string exactMatchText;
    std::string rankText;
    std::string productIdText;


    if (!std::getline(stream, version, '|')) {
        return std::nullopt;
    }

    if (!std::getline(stream, exactMatchText, '|')) {
        return std::nullopt;
    }

    if (!std::getline(stream, rankText, '|')) {
        return std::nullopt;
    }

    if (!std::getline(stream, productIdText, '|')) {
        return std::nullopt;
    }


    /*
     * No deben existir campos adicionales.
     */
    std::string extra;

    if (std::getline(stream, extra, '|')) {
        return std::nullopt;
    }


    /*
     * Permite cambiar el formato en el futuro
     * sin romper tokens anteriores accidentalmente.
     */
    if (version != "v1") {
        return std::nullopt;
    }


    bool exactMatch;

    if (exactMatchText == "1") {
        exactMatch = true;
    }
    else if (exactMatchText == "0") {
        exactMatch = false;
    }
    else {
        return std::nullopt;
    }


    try {

        std::size_t rankPos = 0;

        const float rank =
            std::stof(
                rankText,
                &rankPos
            );

        if (rankPos != rankText.size()) {
            return std::nullopt;
        }


        std::size_t idPos = 0;

        const std::int64_t productId =
            std::stoll(
                productIdText,
                &idPos
            );

        if (idPos != productIdText.size()) {
            return std::nullopt;
        }


        if (rank < 0.0f) {
            return std::nullopt;
        }

        if (productId <= 0) {
            return std::nullopt;
        }


        return ProductSearchCursor{
            .exact_match = exactMatch,
            .rank = rank,
            .product_id = productId
        };

    }
    catch (...) {
        return std::nullopt;
    }
}

}
