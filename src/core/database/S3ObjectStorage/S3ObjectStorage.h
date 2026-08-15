#pragma once

#include "core/database/S3ObjectStorage/OjectStorage.h"
#include "core/database/S3ObjectStorage/S3Config.h"

#include <aws/s3/S3Client.h>

#include <string>

namespace puntodeventa::storage {

	class S3ObjectStorage final : public ObjectStorage {
		public:
			explicit S3ObjectStorage(const S3Config& config);

			std::string putObject(
					const std::string& key,
					std::string_view data,
					const std::string& contentType
					) override;

		private:
			Aws::S3::S3Client client_;
			std::string bucket_;
	};

}
