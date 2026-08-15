#include <string>
#include <random>

namespace puntodeventa {

	std::string generadorBarcode(){

		static std::random_device rd;
		static std::mt19937_64 generator{rd()};
		static std::uniform_int_distribution<int> distribution{0, 9};

		std::string barcode;
		barcode.reserve(13);

		// Generar los primeros 12 dígitos.
		for (int i = 0; i < 12; ++i) {
			barcode += static_cast<char>(
					'0' + distribution(generator)
					);
		}

		// Checksum EAN-13.
		int suma = 0;

		for (int i = 0; i < 12; ++i) {
			const int digito = barcode[i] - '0';

			// Posiciones EAN 1,3,5... peso 1.
			// Posiciones EAN 2,4,6... peso 3.
			if (i % 2 == 0) {
				suma += digito;
			} else {
				suma += digito * 3;
			}
		}

		const int checksum =
			(10 - (suma % 10)) % 10;

		barcode += static_cast<char>(
				'0' + checksum
				);

		return barcode;
	}

}
