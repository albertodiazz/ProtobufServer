#pragma once

#include <string>

struct Thumbnail {
    // Bytes de la imagen codificada; no es texto ni Base64.
    std::string data;

    std::string extension = ".webp";
    std::string contentType = "image/webp";

    int width = 0;
    int height = 0;
};

class ThumbnailGenerator {
public:
    ThumbnailGenerator();

    [[nodiscard]] Thumbnail generate(
        const std::string& originalBytes,
        int maxSide = 512,
        int quality = 80
    ) const;
};
