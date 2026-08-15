#include "core/database/S3ObjectStorage/S3ObjectStorage.h"

#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/s3/model/PutObjectRequest.h>

#include <stdexcept>

namespace puntodeventa::storage {

S3ObjectStorage::S3ObjectStorage(
		const S3Config& config
)
    : client_(
        Aws::Auth::AWSCredentials{
            config.accessKey.c_str(),
            config.secretKey.c_str()
        },
				nullptr, // endpointProvider
        [&]() {
            Aws::S3::S3ClientConfiguration clientConfig;

            clientConfig.endpointOverride = config.endpoint.c_str();
            clientConfig.scheme = Aws::Http::Scheme::HTTP;
            clientConfig.region = "us-east-1";

            // Importante para un S3 local/LAN.
            clientConfig.useVirtualAddressing = false;

            return clientConfig;
        }()
    ),
    bucket_(config.bucket)
{
}

std::string S3ObjectStorage::putObject(
    const std::string& key,
    std::string_view data,
    const std::string& contentType
) {
    // std::cout << "[S3-1] putObject inicio\n";
    // std::cout << "[S3-2] bucket: " << bucket_ << '\n';
    // std::cout << "[S3-3] key: " << key << '\n';
    // std::cout << "[S3-4] contentType: " << contentType << '\n';
    // std::cout << "[S3-5] size: " << data.size() << '\n';

    Aws::S3::Model::PutObjectRequest request;

    request.SetBucket(bucket_.c_str());
    request.SetKey(key.c_str());
    request.SetContentType(contentType.c_str());

    auto stream =
        Aws::MakeShared<Aws::StringStream>("RustFSUpload");

    stream->write(
        data.data(),
        static_cast<std::streamsize>(data.size())
    );

    std::cout << "[S3-6] stream escrito\n";

    request.SetBody(stream);

    std::cout << "[S3-7] llamando PutObject\n";

    auto outcome = client_.PutObject(request);

    std::cout << "[S3-8] PutObject regreso\n";

    if (!outcome.IsSuccess()) {
        const auto& error = outcome.GetError();

        std::cerr
            << "S3 Exception: "
            << error.GetExceptionName()
            << '\n';

        std::cerr
            << "S3 Message: "
            << error.GetMessage()
            << '\n';

        std::cerr
            << "HTTP code: "
            << static_cast<int>(error.GetResponseCode())
            << '\n';

        throw std::runtime_error(
            "Error subiendo objeto a S3: " +
            std::string(error.GetMessage().c_str())
        );
    }

    std::cout << "[S3-9] Upload OK\n";

    return key;
}

}
