#pragma once

#include <engine/core/error.hpp>
#include <engine/core/math.hpp>
#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>
#include <engine/render/pbr_renderer.hpp>

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
    std::uint64_t gpu_samples{0};
    bool gpu_timing_supported{false};
};

[[nodiscard]] constexpr bool default_debug_overlay() noexcept {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

// Presentation-only camera pose. It is supplied by the application and never
// written into the authoritative scene.
struct CameraPose {
    core::Vec3d position{};
    core::Quatd rotation{};
};

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

    // Uses a presentation-only camera pose instead of the scene's active
    // camera transform. Projection parameters still come from the scene camera.
    [[nodiscard]] core::Result<bool> draw_frame(const scene::SceneView& view,
                                                const CameraPose& camera);

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
    [[nodiscard]] bool overlay_visible() const noexcept;
    void set_overlay_visible(bool visible) noexcept;
    // Number of entities the overlay listed on the last frame it was built.
    [[nodiscard]] std::uint64_t overlay_entity_count() const noexcept;
    // Vertices submitted by the overlay on the last frame it was built.
    [[nodiscard]] std::uint32_t overlay_draw_vertices() const noexcept;
    // Routes an SDL event to ImGui. The application should call this before
    // handling the same event itself.
    void process_sdl_event(const SDL_Event& event) noexcept;
    [[nodiscard]] bool overlay_wants_mouse() const noexcept;
    [[nodiscard]] bool overlay_wants_keyboard() const noexcept;

    [[nodiscard]] core::Result<void> upload_asset(const asset::LoadedAsset& asset);
    [[nodiscard]] core::Result<void> retire_asset(std::string_view asset_id);
    [[nodiscard]] core::Result<void> collect_assets();
    [[nodiscard]] PbrStats pbr_stats() const;

private:
    struct Impl;

    explicit Renderer(std::unique_ptr<Impl> impl) noexcept;
    [[nodiscard]] core::Result<bool> draw_frame_impl(const scene::SceneView& view,
                                                     const CameraPose* camera);

    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
