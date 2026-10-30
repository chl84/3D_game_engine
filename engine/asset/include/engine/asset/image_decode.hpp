#pragma once

#include <engine/asset/gltf_import.hpp>
#include <engine/core/error.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace engine::asset {

inline constexpr std::uint32_t kMaxTextureDimension = 8192U;
inline constexpr std::size_t kMaxDecodedTextureBytes = std::size_t{256} * 1024U * 1024U;

struct DecodedImage {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8;
};

// Decodes PNG or JPEG into tightly packed RGBA8. The header is inspected and
// the dimension and decoded-memory limits are enforced before stb_image
// allocates the pixel buffer.
[[nodiscard]] core::Result<DecodedImage> decode_image(std::span<const std::byte> encoded,
                                                      ImageMimeType mime_type);

} // namespace engine::asset
