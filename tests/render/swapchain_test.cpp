#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>
#include <engine/render/renderer.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kSkipExitCode = 77;

int skip(const std::string& message) {
    std::cout << "SKIPPED: " << message << '\n';
    return kSkipExitCode;
}

int fail(const std::string& message) {
    std::cerr << "swapchain test failed: " << message << '\n';
    return 1;
}

[[nodiscard]] bool is_environment_error(const engine::core::Error& error) {
    const std::string stage = error.details.value("stage", "");
    return stage == "loader" || stage == "physical-device" || stage == "validation";
}

[[nodiscard]] engine::core::Result<engine::render::Renderer>
create_renderer(const engine::render::Device& device, const engine::platform::Window& window) {
    using engine::render::SwapchainOptions;
    engine::core::Result<engine::render::Renderer> renderer =
        engine::core::make_failure<engine::render::Renderer>(engine::core::make_error(
            engine::core::ErrorCode::Internal, "no present mode was attempted"));

    for (const VkPresentModeKHR mode :
         {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR}) {
        SwapchainOptions options;
        options.present_mode = mode;
        renderer = engine::render::Renderer::create(device, window, options);
        if (renderer) {
            break;
        }
    }
    return renderer;
}

} // namespace

int main(int argc, char** argv) {
    bool require_minimize = false;
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--require-minimize") {
            require_minimize = true;
        }
    }

#ifdef __linux__
    if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) {
        return skip("no display server is available");
    }
#endif

    auto sdl = engine::platform::SdlContext::initialize();
    if (!sdl) {
        return fail("could not initialize SDL: " + sdl.error().message);
    }

    engine::platform::WindowOptions window_options;
    window_options.title = "engine swapchain test";
    window_options.width = 320;
    window_options.height = 240;
    window_options.resizable = true;
    window_options.hidden = true;
    window_options.high_pixel_density = false;

    auto window = engine::platform::Window::create(*sdl, window_options);
    if (!window) {
        return fail("could not create the window: " + window.error().message);
    }

    engine::render::DeviceOptions device_options;
    device_options.application_name = "engine swapchain test";
    device_options.validation = engine::render::ValidationMode::Required;
    device_options.window = &*window;

    auto device = engine::render::Device::create(device_options);
    if (!device) {
        if (is_environment_error(device.error())) {
            return skip("Vulkan environment is unavailable: " + device.error().message);
        }
        return fail("could not create the Vulkan device: " + device.error().message + " " +
                    device.error().hint);
    }

    auto renderer = create_renderer(*device, *window);
    if (!renderer) {
        return fail("could not create the renderer: " + renderer.error().message + " " +
                    renderer.error().hint);
    }
    if (!renderer->has_swapchain()) {
        return fail("the renderer did not create a swapchain");
    }

    const VkExtent2D initial_extent = renderer->extent();
    if (initial_extent.width == 0 || initial_extent.height == 0) {
        return fail("the initial swapchain extent is empty");
    }

    for (int frame = 0; frame < 3; ++frame) {
        auto drawn = renderer->draw_frame();
        if (!drawn) {
            return fail("could not draw frame " + std::to_string(frame) + ": " +
                        drawn.error().message);
        }
        if (!*drawn) {
            return fail("frame " + std::to_string(frame) + " was not presented");
        }
    }

    auto resized = window->resize(640, 360);
    if (!resized) {
        return fail("could not resize the window: " + resized.error().message);
    }
    sdl->pump_events();

    bool grew = false;
    for (int frame = 0; frame < 60; ++frame) {
        auto drawn = renderer->draw_frame();
        if (!drawn) {
            return fail("could not draw after resize: " + drawn.error().message);
        }
        if (renderer->extent().width > initial_extent.width &&
            std::cmp_equal(renderer->extent().width, window->width()) &&
            std::cmp_equal(renderer->extent().height, window->height())) {
            grew = true;
            break;
        }
    }
    if (!grew) {
        return fail("the swapchain did not follow the window resize");
    }

    auto shown = window->show();
    if (!shown) {
        return fail("could not show the window: " + shown.error().message);
    }
    sdl->pump_events();

    bool followed_mapping = false;
    for (int frame = 0; frame < 120; ++frame) {
        auto drawn = renderer->draw_frame();
        if (!drawn) {
            return fail("could not draw after showing the window: " + drawn.error().message);
        }
        if (std::cmp_equal(renderer->extent().width, window->width()) &&
            std::cmp_equal(renderer->extent().height, window->height())) {
            followed_mapping = true;
            break;
        }
        sdl->pump_events();
    }
    if (!followed_mapping) {
        return fail("the swapchain did not follow the mapped window");
    }

    auto minimized = window->minimize();
    if (!minimized) {
        return fail("could not minimize the window: " + minimized.error().message);
    }
    sdl->pump_events();

    if (window->minimized()) {
        bool reported_no_frame = false;
        for (int frame = 0; frame < 10; ++frame) {
            auto drawn = renderer->draw_frame();
            if (!drawn) {
                return fail("minimized window produced an error: " + drawn.error().message);
            }
            if (!*drawn) {
                reported_no_frame = true;
            } else if (window->minimized()) {
                return fail("the renderer presented while the window was minimized");
            }
            sdl->pump_events();
        }
        if (!reported_no_frame) {
            return fail("the renderer kept presenting while the window was minimized");
        }
        std::cout << "minimize: renderer reported no frames\n";
    } else if (require_minimize) {
        return fail("the window manager did not report a minimized window");
    } else {
        std::cout << "NOTE: window manager ignored minimize; the minimize guard was not asserted\n";
    }

    auto restored = window->restore();
    if (!restored) {
        return fail("could not restore the window: " + restored.error().message);
    }
    sdl->pump_events();

    bool presented_after_restore = false;
    for (int frame = 0; frame < 120; ++frame) {
        auto drawn = renderer->draw_frame();
        if (!drawn) {
            return fail("could not draw after restore: " + drawn.error().message);
        }
        if (*drawn && std::cmp_equal(renderer->extent().width, window->width()) &&
            std::cmp_equal(renderer->extent().height, window->height())) {
            presented_after_restore = true;
            break;
        }
        sdl->pump_events();
    }
    if (!presented_after_restore) {
        return fail("the renderer did not present after the window was restored");
    }

    auto idle = renderer->wait_idle();
    if (!idle) {
        return fail("could not wait for the device: " + idle.error().message);
    }

    const engine::render::ValidationReport report = device->validation_report();
    if (report.errors != 0) {
        for (const std::string& message : report.messages) {
            std::cerr << message << '\n';
        }
        return fail("the validation layers reported errors");
    }

    std::cout << "presented " << renderer->presented_frames() << " frames, extent "
              << renderer->extent().width << 'x' << renderer->extent().height << ", "
              << renderer->image_count() << " images\n";
    return 0;
}
