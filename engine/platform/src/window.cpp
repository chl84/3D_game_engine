#include <engine/platform/window.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <utility>

namespace engine::platform {
namespace {

core::Error sdl_error(std::string message) {
    return core::make_error(core::ErrorCode::Precondition, std::move(message), SDL_GetError());
}

} // namespace

core::Result<SdlContext> SdlContext::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return core::make_failure<SdlContext>(sdl_error("could not initialize SDL video"));
    }

    SdlContext context;
    context.active_ = true;
    return context;
}

SdlContext::SdlContext(SdlContext&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}

SdlContext& SdlContext::operator=(SdlContext&& other) noexcept {
    if (this != &other) {
        if (active_) {
            SDL_Quit();
        }
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}

SdlContext::~SdlContext() {
    if (active_) {
        SDL_Quit();
    }
}

bool SdlContext::active() const noexcept {
    return active_;
}

void SdlContext::pump_events() noexcept {
    SDL_PumpEvents();
}

bool SdlContext::poll_event(SDL_Event& event) noexcept {
    return SDL_PollEvent(&event) == 1;
}

core::Result<Window> Window::create(const SdlContext& context, const WindowOptions& options) {
    if (!context.active()) {
        return core::make_failure<Window>(core::make_error(core::ErrorCode::Precondition,
                                                           "SDL context is not initialized", {},
                                                           {{"stage", "sdl-context"}}));
    }
    if (options.width <= 0 || options.height <= 0) {
        return core::make_failure<Window>(
            core::make_error(core::ErrorCode::Validation, "window size must be positive", {},
                             {{"width", options.width}, {"height", options.height}}));
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
    if (options.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (options.hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    if (options.high_pixel_density) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    SDL_Window* handle =
        SDL_CreateWindow(options.title.c_str(), options.width, options.height, flags);
    if (handle == nullptr) {
        return core::make_failure<Window>(sdl_error("could not create the SDL window"));
    }

    Window window(handle);
    int pixel_width = options.width;
    int pixel_height = options.height;
    if (SDL_GetWindowSizeInPixels(handle, &pixel_width, &pixel_height)) {
        window.width_ = pixel_width;
        window.height_ = pixel_height;
    } else {
        window.width_ = options.width;
        window.height_ = options.height;
    }
    return window;
}

Window::Window(SDL_Window* window) noexcept : window_(window) {}

Window::Window(Window&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)), width_(other.width_), height_(other.height_) {
    other.width_ = 0;
    other.height_ = 0;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        window_ = std::exchange(other.window_, nullptr);
        width_ = other.width_;
        height_ = other.height_;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

Window::~Window() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
}

core::Result<std::vector<std::string>> Window::vulkan_instance_extensions() const {
    if (window_ == nullptr) {
        return core::make_failure<std::vector<std::string>>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "surface"}}));
    }

    Uint32 count = 0;
    char const* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (names == nullptr) {
        return core::make_failure<std::vector<std::string>>(
            sdl_error("could not query the Vulkan instance extensions"));
    }

    std::vector<std::string> extensions;
    extensions.reserve(count);
    for (Uint32 index = 0; index < count; ++index) {
        extensions.emplace_back(names[index]);
    }
    return extensions;
}

core::Result<VkSurfaceKHR> Window::create_vulkan_surface(VkInstance instance) const {
    if (window_ == nullptr) {
        return core::make_failure<VkSurfaceKHR>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "surface"}}));
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface)) {
        return core::make_failure<VkSurfaceKHR>(sdl_error("could not create the Vulkan surface"));
    }
    return surface;
}

SDL_Window* Window::native_handle() const noexcept {
    return window_;
}

core::Result<void> Window::resize(int width, int height) {
    if (window_ == nullptr) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "window"}}));
    }
    if (width <= 0 || height <= 0) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Validation,
                                                         "window size must be positive", {},
                                                         {{"width", width}, {"height", height}}));
    }
    if (!SDL_SetWindowSize(window_, width, height)) {
        return core::make_failure<void>(sdl_error("could not resize the SDL window"));
    }
    (void)SDL_SyncWindow(window_);
    width_ = width;
    height_ = height;
    return {};
}

core::Result<void> Window::show() {
    if (window_ == nullptr) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "window"}}));
    }
    if (!SDL_ShowWindow(window_)) {
        return core::make_failure<void>(sdl_error("could not show the SDL window"));
    }
    (void)SDL_SyncWindow(window_);
    return {};
}

core::Result<void> Window::minimize() {
    if (window_ == nullptr) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "window"}}));
    }
    if (!SDL_MinimizeWindow(window_)) {
        return core::make_failure<void>(sdl_error("could not minimize the SDL window"));
    }
    (void)SDL_SyncWindow(window_);
    return {};
}

core::Result<void> Window::restore() {
    if (window_ == nullptr) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Precondition, "window is not valid", {}, {{"stage", "window"}}));
    }
    if (!SDL_RestoreWindow(window_)) {
        return core::make_failure<void>(sdl_error("could not restore the SDL window"));
    }
    (void)SDL_SyncWindow(window_);
    return {};
}

int Window::width() const noexcept {
    int width = width_;
    int height = height_;
    if (window_ != nullptr && SDL_GetWindowSizeInPixels(window_, &width, &height)) {
        return width;
    }
    return width_;
}

int Window::height() const noexcept {
    int width = width_;
    int height = height_;
    if (window_ != nullptr && SDL_GetWindowSizeInPixels(window_, &width, &height)) {
        return height;
    }
    return height_;
}

bool Window::minimized() const noexcept {
    if (window_ == nullptr) {
        return true;
    }
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    return (flags & SDL_WINDOW_MINIMIZED) != 0 || width() == 0 || height() == 0;
}

} // namespace engine::platform
