#include <engine/asset/gltf_import.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);
    auto result = engine::asset::import_glb(bytes, "fuzz.glb");
    if (result) {
        (void)engine::asset::world_transforms(*result);
    }
    return 0;
}
