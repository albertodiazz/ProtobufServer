#include "core/image/ImageFormatDetector.h"

namespace puntodeventa::image {

	ImageFormat detectImageFormat(
			std::string_view data
			) {
		// PNG:
		// 89 50 4E 47 0D 0A 1A 0A
		if (
				data.size() >= 8 &&
				static_cast<unsigned char>(data[0]) == 0x89 &&
				static_cast<unsigned char>(data[1]) == 0x50 &&
				static_cast<unsigned char>(data[2]) == 0x4E &&
				static_cast<unsigned char>(data[3]) == 0x47 &&
				static_cast<unsigned char>(data[4]) == 0x0D &&
				static_cast<unsigned char>(data[5]) == 0x0A &&
				static_cast<unsigned char>(data[6]) == 0x1A &&
				static_cast<unsigned char>(data[7]) == 0x0A
			 ) {
			return ImageFormat::PNG;
		}

		// JPEG:
		// FF D8 FF
		if (
				data.size() >= 3 &&
				static_cast<unsigned char>(data[0]) == 0xFF &&
				static_cast<unsigned char>(data[1]) == 0xD8 &&
				static_cast<unsigned char>(data[2]) == 0xFF
			 ) {
			return ImageFormat::JPEG;
		}

		// WEBP:
		// RIFF .... WEBP
		if (
				data.size() >= 12 &&
				data[0] == 'R' &&
				data[1] == 'I' &&
				data[2] == 'F' &&
				data[3] == 'F' &&
				data[8] == 'W' &&
				data[9] == 'E' &&
				data[10] == 'B' &&
				data[11] == 'P'
			 ) {
			return ImageFormat::WEBP;
		}

		return ImageFormat::UNKNOWN;
	}

}
