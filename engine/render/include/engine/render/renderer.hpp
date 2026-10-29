#pragma once

#include <engine/core/error.hpp>
#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>

#include <volk.h>

#include <cstdint>
#include <memory>

namespace engine::scene {
class SceneView;
}

namespace engine::render {

// Debug-only frame statistics. They are read from the previous completed
// frame, so the overlay can display them without stalling the GPU.
struct FrameStats {
    std::uint64_t frames{0};
    double frames_per_second{0.0};
    double cpu_frame_ms{0.0};
    double gpu_frame_ms{0.0};
    bool gpu_timing_supported{false};
};

[[nodiscard]] constexpr bool default_debug_overlay() noexcept {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

struct SwapchainOptions {
    VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
    // The overlay is a debug aid. It never mutates the scene and is disabled
    // by default in release builds.
    bool debug_overlay{default_debug_overlay()};
};

// Clears and presents a swapchain attached to a window. The device and the
// window must outlive the renderer. The renderer is single-threaded.
class Renderer {
public:
    [[nodiscard]] static core::Result<Renderer> create(const Device& device,
                                                       const platform::Window& window,
                                                       const SwapchainOptions& options = {});

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    ~Renderer();

    // Acquires, renders the scene view and presents one frame. Returns false
    // when the window is minimized or a new swapchain could not be created yet.
    [[nodiscard]] core::Result<bool> draw_frame(const scene::SceneView& view);

    [[nodiscard]] core::Result<void> wait_idle() const;

    [[nodiscard]] bool has_swapchain() const noexcept;
    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] VkFormat color_format() const noexcept;
    [[nodiscard]] std::uint32_t image_count() const noexcept;
    [[nodiscard]] std::uint64_t presented_frames() const noexcept;
    [[nodiscard]] std::uint32_t last_visible_entities() const noexcept;

    // Returns statistics from the last completed frame. FrameStats::frames
    // follows presented_frames().
    [[nodiscard]] FrameStats frame_stats() const noexcept;
    [[nodiscard]] bool overlay_enabled() const noexcept;
    // Number of entities the overlay listed on the last frame it was built.
    [[nodiscard]] std::uint64_t overlay_entity_count() const noexcept;
    // Vertices submitted by the overlay on the last frame it was built.
    [[nodiscard]] std::uint32_t overlay_draw_vertices() const noexcept;

private:
    struct Impl;

    explicit Renderer(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
