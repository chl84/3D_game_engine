#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <engine/asset/image_decode.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace engine::asset {
namespace {

[[nodiscard]] core::Error decode_error(std::string message, std::string reason = {}) {
    return core::make_error(core::ErrorCode::Validation, std::move(message),
                            "Use a valid PNG or JPEG texture.", {{"reason", std::move(reason)}});
}

// PNG permits empty IDAT chunks, including before the compressed data. The
// pinned stb reader passes its still-null data buffer to memcpy for these.
// Remove only leading empty IDAT chunks: they carry no compressed bytes.
// Preserve the original header/chunks/CRCs and let stb validate/decode the rest.
// No dependency patch or rejection of valid empty-chunk PNGs is needed.
[[nodiscard]] std::vector<std::byte> normalize_png(std::span<const std::byte> encoded) {
    std::vector<std::byte> output;
    std::size_t copied = 0;
    std::size_t cursor = 8; // stbi_info has already validated the PNG signature.
    while (cursor <= encoded.size() && encoded.size() - cursor >= 12U) {
        std::uint32_t length = 0;
        for (std::size_t index = 0; index < 4U; ++index) {
            length = (length << 8U) | std::to_integer<std::uint8_t>(encoded[cursor + index]);
        }
        if (length > encoded.size() - cursor - 12U) {
            break; // Truncated/invalid chunk: leave it for the existing decoder.
        }
        const bool idat =
            encoded[cursor + 4U] == std::byte{'I'} && encoded[cursor + 5U] == std::byte{'D'} &&
            encoded[cursor + 6U] == std::byte{'A'} && encoded[cursor + 7U] == std::byte{'T'};
        if (idat && length != 0U) {
            break; // stb allocates its data buffer at the first nonempty IDAT.
        }
        if (idat) {
            if (output.empty()) {
                output.reserve(encoded.size());
            }
            const auto prefix = encoded.subspan(copied, cursor - copied);
            output.insert(output.end(), prefix.begin(), prefix.end());
            copied = cursor + 12U;
        }
        cursor += static_cast<std::size_t>(length) + 12U;
    }
    if (!output.empty()) {
        const auto remainder = encoded.subspan(copied);
        output.insert(output.end(), remainder.begin(), remainder.end());
    }
    return output;
}

} // namespace

core::Result<DecodedImage> decode_image(std::span<const std::byte> encoded,
                                        ImageMimeType mime_type) {
    if (encoded.empty()) {
        return core::make_failure<DecodedImage>(decode_error("image is empty"));
    }
    const auto starts_with = [encoded](std::initializer_list<std::uint8_t> prefix) {
        if (encoded.size() < prefix.size()) {
            return false;
        }
        std::size_t index = 0;
        for (const std::uint8_t value : prefix) {
            if (std::to_integer<std::uint8_t>(encoded[index]) != value) {
                return false;
            }
            ++index;
        }
        return true;
    };
    const bool is_png = starts_with({0x89U, 0x50U, 0x4EU, 0x47U});
    const bool is_jpeg = starts_with({0xFFU, 0xD8U, 0xFFU});
    if ((mime_type == ImageMimeType::Png && !is_png) ||
        (mime_type == ImageMimeType::Jpeg && !is_jpeg)) {
        return core::make_failure<DecodedImage>(
            decode_error("image data does not match the declared MIME type"));
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoded.data());
    const auto size = static_cast<int>(std::min<std::size_t>(
        encoded.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    if (stbi_info_from_memory(bytes, size, &width, &height, &channels) == 0) {
        const char* reason = stbi_failure_reason();
        return core::make_failure<DecodedImage>(
            decode_error("image header could not be decoded", reason != nullptr ? reason : ""));
    }
    if (width <= 0 || height <= 0) {
        return core::make_failure<DecodedImage>(decode_error("image dimensions are invalid"));
    }
    if (std::cmp_greater(width, kMaxTextureDimension) ||
        std::cmp_greater(height, kMaxTextureDimension)) {
        return core::make_failure<DecodedImage>(core::make_error(
            core::ErrorCode::Validation, "image exceeds the texture dimension limit",
            "Resize the texture to at most 8192x8192.",
            {
                {"width", width},
                {"height", height},
                {"maxDimension", kMaxTextureDimension},
                {"mimeType", mime_type == ImageMimeType::Png ? "png" : "jpeg"},
            }));
    }
    const auto pixel_count = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixel_count > kMaxDecodedTextureBytes / 4U) {
        return core::make_failure<DecodedImage>(
            core::make_error(core::ErrorCode::Validation, "image exceeds the decoded memory limit",
                             "Resize the texture to at most 256 MiB of RGBA8 data.",
                             {
                                 {"width", width},
                                 {"height", height},
                                 {"decodedBytes", pixel_count * 4U},
                                 {"limit", kMaxDecodedTextureBytes},
                             }));
    }
    int decoded_channels = 0;
    const auto normalized = is_png ? normalize_png(encoded.first(static_cast<std::size_t>(size)))
                                   : std::vector<std::byte>{};
    const auto* decode_bytes =
        normalized.empty() ? bytes : reinterpret_cast<const unsigned char*>(normalized.data());
    const auto decode_size = normalized.empty() ? size : static_cast<int>(normalized.size());
    unsigned char* pixels =
        stbi_load_from_memory(decode_bytes, decode_size, &width, &height, &decoded_channels, 4);
    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        return core::make_failure<DecodedImage>(
            decode_error("image could not be decoded", reason != nullptr ? reason : ""));
    }
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    DecodedImage image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.rgba8.assign(pixels, pixels + expected);
    stbi_image_free(pixels);
    return image;
}

} // namespace engine::asset
