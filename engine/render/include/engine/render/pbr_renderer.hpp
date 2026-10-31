#pragma once

#include <engine/asset/asset_library.hpp>
#include <engine/render/device.hpp>
#include <engine/scene/scene_view.hpp>
#include <memory>
#include <string_view>

namespace engine::render {

struct PbrStats {
    std::uint32_t textures{0};
    std::uint32_t materials{0};
    std::uint32_t retired_assets{0};
};

// Shared read-only asset rendering for swapchain and offscreen targets.
// Device outlives this object. Upload/collection are synchronous safe points;
// retire does not free or reuse descriptors while submitted frames can use them.
class PbrRenderer {
public:
    static core::Result<PbrRenderer> create(const Device& device, std::uint32_t frames);
    ~PbrRenderer();
    PbrRenderer(PbrRenderer&&) noexcept;
    PbrRenderer& operator=(PbrRenderer&&) noexcept;
    PbrRenderer(const PbrRenderer&) = delete;
    PbrRenderer& operator=(const PbrRenderer&) = delete;
    core::Result<void> upload(const asset::LoadedAsset& source);
    core::Result<void> retire(std::string_view asset_id);
    core::Result<void> collect();
    core::Result<void> set_format(VkFormat color, VkFormat depth);
    // Caller has waited this frame's fence. Call before beginning rendering.
    core::Result<void> prepare(const scene::SceneView& view, const scene::WorldTransform& camera,
                               float aspect, std::uint32_t frame);
    void draw(VkCommandBuffer cmd, std::uint32_t frame) const;
    PbrStats stats() const;

private:
    struct Impl;
    explicit PbrRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
