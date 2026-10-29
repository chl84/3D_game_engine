#pragma once

#include <engine/core/error.hpp>
#include <engine/render/device.hpp>
#include <engine/scene/scene_view.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace engine::render {

struct OffscreenOptions {
    std::uint32_t width{1280};
    std::uint32_t height{720};
};

// Renders a scene to an offscreen RGBA8 image without a window or swapchain.
// The device must outlive the renderer.
class OffscreenRenderer {
public:
    [[nodiscard]] static core::Result<OffscreenRenderer> create(const Device& device,
                                                                const OffscreenOptions& options);

    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;
    OffscreenRenderer(OffscreenRenderer&&) noexcept;
    OffscreenRenderer& operator=(OffscreenRenderer&&) noexcept;
    ~OffscreenRenderer();

    // Renders the scene for the requested number of frames and returns the
    // final RGBA8 image, top-left first.
    [[nodiscard]] core::Result<std::vector<std::uint8_t>> render(const scene::SceneView& view,
                                                                 int frames);

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

private:
    struct Impl;

    explicit OffscreenRenderer(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
