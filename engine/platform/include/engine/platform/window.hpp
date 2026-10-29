#pragma once

#include <engine/core/error.hpp>

#include <volk.h>

#include <string>
#include <vector>

struct SDL_Window;

namespace engine::platform {

struct WindowOptions {
    std::string title{"Engine"};
    int width{1280};
    int height{720};
    bool resizable{true};
    bool hidden{false};
    bool high_pixel_density{true};
};

// Owns the SDL video and event subsystems. The context must outlive every
// window created from it.
class SdlContext {
public:
    [[nodiscard]] static core::Result<SdlContext> initialize();

    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
    SdlContext(SdlContext&& other) noexcept;
    SdlContext& operator=(SdlContext&& other) noexcept;
    ~SdlContext();

    [[nodiscard]] bool active() const noexcept;

private:
    SdlContext() = default;

    bool active_{false};
};

// A Vulkan-capable SDL window. The window must outlive the surface created
// from it.
class Window {
public:
    [[nodiscard]] static core::Result<Window> create(const SdlContext& context,
                                                     const WindowOptions& options);

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;
    ~Window();

    [[nodiscard]] core::Result<std::vector<std::string>> vulkan_instance_extensions() const;
    [[nodiscard]] core::Result<VkSurfaceKHR> create_vulkan_surface(VkInstance instance) const;

    [[nodiscard]] SDL_Window* native_handle() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

private:
    explicit Window(SDL_Window* window) noexcept;

    SDL_Window* window_{nullptr};
    int width_{0};
    int height_{0};
};

} // namespace engine::platform
