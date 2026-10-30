#include "commands.hpp"
#include "scene_assets.hpp"
#include "scene_warnings.hpp"

#include <engine/command/command.hpp>
#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>
#include <engine/render/renderer.hpp>
#include <engine/scene/project.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/scene_view.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

namespace engine::cli {
namespace {

using command::CommandOutput;
using command::CommandResult;
using core::Error;
using core::ErrorCode;
using core::ReflectionRegistry;
using scene::Project;

[[nodiscard]] CommandResult success(nlohmann::json data, std::vector<std::string> warnings = {}) {
    return CommandResult::success(CommandOutput{std::move(data), std::move(warnings)});
}

[[nodiscard]] CommandResult failure(Error error) {
    return CommandResult::failure(std::move(error));
}

void print_result(const CommandResult& result, bool json) {
    if (json) {
        std::cout << result.to_json().dump(2) << '\n';
        return;
    }
    if (result.ok) {
        std::cout << result.data.dump(2) << '\n';
    } else if (result.error.has_value()) {
        std::cerr << "error: " << result.error->message << '\n';
        if (!result.error->hint.empty()) {
            std::cerr << "hint: " << result.error->hint << '\n';
        }
    }
    for (const std::string& warning : result.warnings) {
        std::cerr << "warning: " << warning << '\n';
    }
}

[[nodiscard]] int exit_code_for(const CommandResult& result) {
    if (result.ok) {
        return 0;
    }
    return result.error.has_value() ? core::exit_code(result.error->code) : 1;
}

[[nodiscard]] core::Quatd camera_rotation(double yaw, double pitch) {
    const core::Quatd yaw_rotation{0.0, std::sin(yaw * 0.5), 0.0, std::cos(yaw * 0.5)};
    const core::Quatd pitch_rotation{std::sin(pitch * 0.5), 0.0, 0.0, std::cos(pitch * 0.5)};
    return core::normalize(yaw_rotation * pitch_rotation);
}

[[nodiscard]] core::Vec3d normalized(core::Vec3d value) {
    const double length =
        std::sqrt((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
    if (length <= 0.0) {
        return {};
    }
    return value * (1.0 / length);
}

[[nodiscard]] bool key_down(const std::array<bool, SDL_SCANCODE_COUNT>& keys, SDL_Scancode key) {
    return keys[static_cast<std::size_t>(key)];
}

} // namespace

int run_window(const std::string& project_root, const std::string& scene_path, int frames,
               int width, int height, bool hidden, bool json) {
    constexpr int kMaxDimension = 8192;
    constexpr int kMaxFrames = 100000;
    if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension ||
        frames < 0 || frames > kMaxFrames) {
        const CommandResult result = failure(
            core::make_error(ErrorCode::Validation, "invalid run dimensions or frame count", {},
                             {{"frames", frames}, {"width", width}, {"height", height}}));
        print_result(result, json);
        return exit_code_for(result);
    }

    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry;
    scene::register_scene_components(registry);
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        const CommandResult result = failure(scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const scene::SceneView view(*scene);
    const auto warnings = point_light_warnings(*scene, scene_path);
    const auto camera_entity = view.active_camera();
    if (!camera_entity.has_value()) {
        const CommandResult result = failure(core::make_error(
            ErrorCode::NotFound, "the scene has no active camera", {}, {{"stage", "scene"}}));
        print_result(result, json);
        return exit_code_for(result);
    }

    const scene::WorldTransform camera_world = view.world_transform(*camera_entity);
    const core::Vec3d initial_forward =
        core::rotate(camera_world.rotation, core::Vec3d{0.0, 0.0, -1.0});
    double yaw = std::atan2(-initial_forward.x, -initial_forward.z);
    double pitch = std::asin(std::clamp(initial_forward.y, -1.0, 1.0));
    core::Vec3d position = camera_world.position;

    auto sdl = platform::SdlContext::initialize();
    if (!sdl) {
        const CommandResult result = failure(sdl.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    platform::WindowOptions window_options;
    window_options.title = "engine run";
    window_options.width = width;
    window_options.height = height;
    window_options.resizable = true;
    window_options.hidden = hidden;
    window_options.high_pixel_density = true;
    auto window = platform::Window::create(*sdl, window_options);
    if (!window) {
        const CommandResult result = failure(window.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    render::DeviceOptions device_options;
    device_options.application_name = "engine run";
    device_options.window = &*window;
    auto device = render::Device::create(device_options);
    if (!device) {
        const CommandResult result = failure(device.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    render::SwapchainOptions swapchain_options;
    swapchain_options.debug_overlay = true;
    auto renderer = render::Renderer::create(*device, *window, swapchain_options);
    if (!renderer) {
        const CommandResult result = failure(renderer.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    const auto uploaded = upload_scene_assets(*renderer, project->root(), view);
    if (!uploaded) {
        const auto result = failure(uploaded.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    std::array<bool, SDL_SCANCODE_COUNT> keys{};
    bool running = true;
    bool looking = false;
    double last_time = 0.0;
    const auto clock_start = std::chrono::steady_clock::now();

    while (running) {
        SDL_Event event;
        while (sdl->poll_event(event)) {
            renderer->process_sdl_event(event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(window->native_handle())) {
                    running = false;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat) {
                    break;
                }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                } else if (event.key.scancode == SDL_SCANCODE_F1) {
                    renderer->set_overlay_visible(!renderer->overlay_visible());
                } else if (event.key.scancode < SDL_SCANCODE_COUNT) {
                    keys[static_cast<std::size_t>(event.key.scancode)] = true;
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (event.key.scancode < SDL_SCANCODE_COUNT) {
                    keys[static_cast<std::size_t>(event.key.scancode)] = false;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_RIGHT && !renderer->overlay_wants_mouse()) {
                    looking = true;
                    (void)SDL_SetWindowRelativeMouseMode(window->native_handle(), true);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    looking = false;
                    (void)SDL_SetWindowRelativeMouseMode(window->native_handle(), false);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (looking) {
                    constexpr double kMouseSensitivity = 0.0025;
                    yaw -= static_cast<double>(event.motion.xrel) * kMouseSensitivity;
                    pitch -= static_cast<double>(event.motion.yrel) * kMouseSensitivity;
                    pitch = std::clamp(pitch, -1.55334, 1.55334);
                }
                break;
            default:
                break;
            }
        }

        const double now =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - clock_start).count();
        const double delta = std::clamp(now - last_time, 0.0, 0.1);
        last_time = now;

        const core::Quatd rotation = camera_rotation(yaw, pitch);
        if (!renderer->overlay_wants_keyboard()) {
            core::Vec3d movement;
            if (key_down(keys, SDL_SCANCODE_W)) {
                movement = movement + core::rotate(rotation, core::Vec3d{0.0, 0.0, -1.0});
            }
            if (key_down(keys, SDL_SCANCODE_S)) {
                movement = movement + core::rotate(rotation, core::Vec3d{0.0, 0.0, 1.0});
            }
            if (key_down(keys, SDL_SCANCODE_A)) {
                movement = movement + core::rotate(rotation, core::Vec3d{-1.0, 0.0, 0.0});
            }
            if (key_down(keys, SDL_SCANCODE_D)) {
                movement = movement + core::rotate(rotation, core::Vec3d{1.0, 0.0, 0.0});
            }
            if (key_down(keys, SDL_SCANCODE_Q)) {
                movement = movement + core::Vec3d{0.0, -1.0, 0.0};
            }
            if (key_down(keys, SDL_SCANCODE_E)) {
                movement = movement + core::Vec3d{0.0, 1.0, 0.0};
            }
            if (!(movement == core::Vec3d{})) {
                const double speed =
                    key_down(keys, SDL_SCANCODE_LSHIFT) || key_down(keys, SDL_SCANCODE_RSHIFT)
                        ? 30.0
                        : 10.0;
                position = position + normalized(movement) * (speed * delta);
            }
        }

        render::CameraPose pose;
        pose.position = position;
        pose.rotation = rotation;
        auto drawn = renderer->draw_frame(view, pose);
        if (!drawn) {
            const CommandResult result = failure(drawn.error());
            print_result(result, json);
            return exit_code_for(result);
        }
        if (frames > 0 && renderer->presented_frames() >= static_cast<std::uint64_t>(frames)) {
            break;
        }
    }

    (void)SDL_SetWindowRelativeMouseMode(window->native_handle(), false);
    auto idle = renderer->wait_idle();
    if (!idle) {
        const CommandResult result = failure(idle.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    const CommandResult result = success(
        {
            {"frames", renderer->presented_frames()},
            {"width", window->width()},
            {"height", window->height()},
        },
        warnings);
    print_result(result, json);
    return exit_code_for(result);
}

} // namespace engine::cli
