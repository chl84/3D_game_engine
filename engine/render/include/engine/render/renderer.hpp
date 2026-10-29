#pragma once

#include <engine/core/error.hpp>
#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>

#include <volk.h>

#include <cstdint>
#include <memory>

namespace engine::render {

struct SwapchainOptions {
    VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
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

    // Acquires, clears and presents one frame. Returns false when the window is
    // minimized or a new swapchain could not be created yet.
    [[nodiscard]] core::Result<bool> draw_frame();

    [[nodiscard]] core::Result<void> wait_idle() const;

    [[nodiscard]] bool has_swapchain() const noexcept;
    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] VkFormat color_format() const noexcept;
    [[nodiscard]] std::uint32_t image_count() const noexcept;
    [[nodiscard]] std::uint64_t presented_frames() const noexcept;

private:
    struct Impl;

    explicit Renderer(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
