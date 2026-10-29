#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kSkipExitCode = 77;

int skip(const std::string& message) {
    std::cout << "SKIPPED: " << message << '\n';
    return kSkipExitCode;
}

int fail(const std::string& message) {
    std::cerr << "window smoke test failed: " << message << '\n';
    return 1;
}

} // namespace

int main() {
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
    window_options.title = "engine window smoke test";
    window_options.width = 320;
    window_options.height = 240;
    window_options.resizable = false;
    window_options.hidden = true;
    window_options.high_pixel_density = false;

    auto window = engine::platform::Window::create(*sdl, window_options);
    if (!window) {
        return fail("could not create the window: " + window.error().message);
    }

    auto extensions = window->vulkan_instance_extensions();
    if (!extensions) {
        return fail("could not query the Vulkan extensions: " + extensions.error().message);
    }
    if (std::find(extensions->begin(), extensions->end(), VK_KHR_SURFACE_EXTENSION_NAME) ==
        extensions->end()) {
        return fail("VK_KHR_surface is missing from the window extensions");
    }

    engine::render::DeviceOptions device_options;
    device_options.application_name = "engine window smoke test";
    device_options.window = &*window;

    auto device = engine::render::Device::create(device_options);
    if (!device) {
        if (device.error().details.value("stage", "") == "loader") {
            return skip("Vulkan loader is not available: " + device.error().message);
        }
        return fail("could not create the Vulkan device: " + device.error().message + " " +
                    device.error().hint);
    }

    if (!device->has_surface()) {
        return fail("the device did not create a surface");
    }
    if (!device->queue_families().presentation) {
        return fail("the device has no present queue");
    }

    const engine::render::ValidationReport report = device->validation_report();
    if (device->validation_enabled() && report.errors != 0) {
        for (const std::string& message : report.messages) {
            std::cerr << message << '\n';
        }
        return fail("the validation layers reported errors");
    }

    return 0;
}
