#include <cstdint>
#include "features/product/domain/Product.h"


namespace puntodeventa::product {


	class ProductRepository {
		public:
			virtual ~ProductRepository() = default;

			virtual int64_t create(const Producto& producto) = 0;

	};

}

