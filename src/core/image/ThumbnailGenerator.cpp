#include "ThumbailGenerator.h"

#include <vips/vips8>

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

namespace {

// Mantiene libvips inicializado durante la vida del proceso.
class VipsRuntime {
public:
    VipsRuntime() {
        if (VIPS_INIT("PuntoDeVenta") != 0) {
            const std::string message = vips_error_buffer();
            vips_error_clear();

            throw std::runtime_error(
                "No se pudo inicializar libvips: " + message
            );
        }
    }

    ~VipsRuntime() {
        vips_shutdown();
    }

    VipsRuntime(const VipsRuntime&) = delete;
    VipsRuntime& operator=(const VipsRuntime&) = delete;
};

// Libera los buffers incluso si se produce una excepción.
struct BlobDeleter {
    void operator()(VipsBlob* blob) const noexcept {
        if (blob != nullptr) {
            g_boxed_free(VIPS_TYPE_BLOB, blob);
        }
    }
};

using BlobPtr = std::unique_ptr<VipsBlob, BlobDeleter>;

} // namespace

ThumbnailGenerator::ThumbnailGenerator() {
    // Se inicializa una sola vez, aunque existan varias instancias.
    static VipsRuntime runtime;
    (void)runtime;
}

Thumbnail ThumbnailGenerator::generate(
    const std::string& originalBytes,
    int maxSide,
    int quality
) const {
    constexpr std::size_t maxInputBytes =
        32U * 1024U * 1024U;

    if (originalBytes.empty()) {
        throw std::invalid_argument(
            "La imagen está vacía."
        );
    }

    if (originalBytes.size() > maxInputBytes) {
        throw std::invalid_argument(
            "La imagen supera 32 MiB."
        );
    }

    if (maxSide < 1 || maxSide > 2048) {
        throw std::invalid_argument(
            "maxSide debe estar entre 1 y 2048."
        );
    }

    if (quality < 1 || quality > 100) {
        throw std::invalid_argument(
            "quality debe estar entre 1 y 100."
        );
    }

    try {
        // Copia administrada por libvips. Evita depender de
        // la vida del buffer original si se retienen operaciones.
        BlobPtr input{
            vips_blob_copy(
                originalBytes.data(),
                originalBytes.size()
            )
        };

        if (!input) {
            throw std::bad_alloc{};
        }

        const auto image = vips::VImage::thumbnail_buffer(
            input.get(),
            maxSide,
            vips::VImage::option()
                ->set("height", maxSide)
                ->set("size", VIPS_SIZE_DOWN)
                ->set("crop", VIPS_INTERESTING_NONE)
                ->set("no_rotate", false)
                ->set("fail_on", VIPS_FAIL_ON_ERROR)
        ).colourspace(VIPS_INTERPRETATION_sRGB);

        // Codifica la miniatura en memoria.
        BlobPtr output{
            image.webpsave_buffer(
                vips::VImage::option()
                    ->set("Q", quality)
                    ->set("lossless", false)
                    ->set("keep", VIPS_FOREIGN_KEEP_NONE)
            )
        };

        if (!output) {
            throw std::runtime_error(
                "No se pudo codificar la miniatura."
            );
        }

        std::size_t size = 0;

        const void* bytes = vips_blob_get(
            output.get(),
            &size
        );

        if (bytes == nullptr || size == 0) {
            throw std::runtime_error(
                "La miniatura generada está vacía."
            );
        }

        Thumbnail result;

        result.data.assign(
            static_cast<const char*>(bytes),
            size
        );

        result.width = image.width();
        result.height = image.height();

        return result;

    } catch (const vips::VError& error) {
        throw std::runtime_error(
            std::string("No se pudo generar la miniatura: ")
            + error.what()
        );
    }
}
