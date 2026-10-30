#include <engine/asset/image_decode.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);
    (void)engine::asset::decode_image(bytes, engine::asset::ImageMimeType::Png);
    (void)engine::asset::decode_image(bytes, engine::asset::ImageMimeType::Jpeg);
    return 0;
}
