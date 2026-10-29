#include <engine/platform/window.hpp>
#include <engine/render/device.hpp>
#include <engine/render/renderer.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/scene_view.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr int kSkipExitCode = 77;

int skip(const std::string& message) {
    std::cout << "SKIPPED: " << message << '\n';
    return kSkipExitCode;
}

int fail(const std::string& message) {
    std::cerr << "overlay test failed: " << message << '\n';
    return 1;
}

[[nodiscard]] bool is_environment_error(const engine::core::Error& error) {
    const std::string stage = error.details.value("stage", "");
    if (stage == "instance") {
        return error.details.value("vkResult", 0) == static_cast<int>(VK_ERROR_INCOMPATIBLE_DRIVER);
    }
    return stage == "loader" || stage == "physical-device" || stage == "validation";
}

[[nodiscard]] std::optional<engine::scene::Scene> build_test_scene() {
    using engine::scene::Camera;
    using engine::scene::Entity;
    using engine::scene::Mesh;
    using engine::scene::Scene;

    Scene scene = Scene::with_seed(23);
    scene.set_scene_name("m2.6-overlay");

    const auto create = [&](const char* name) -> std::optional<Entity> {
        auto entity = scene.create_entity(name);
        if (!entity) {
            return std::nullopt;
        }
        return *entity;
    };
    const auto add_mesh = [&](Entity entity, std::string_view mesh_id) {
        auto added = scene.add_component<Mesh>(entity);
        if (!added) {
            return false;
        }
        scene.component<Mesh>(entity).mesh_id = std::string(mesh_id);
        return true;
    };

    const auto camera = create("Camera");
    const auto cube = create("Cube");
    const auto ground = create("Ground");
    if (!camera || !cube || !ground) {
        return std::nullopt;
    }

    scene.transform(*camera).position = {0.0, 1.0, 5.0};
    auto camera_added = scene.add_component<Camera>(*camera);
    if (!camera_added) {
        return std::nullopt;
    }
    scene.component<Camera>(*camera).fovYRadians = 1.0471975511965976F;
    scene.component<Camera>(*camera).near = 0.1F;
    scene.component<Camera>(*camera).far = 100.0F;

    scene.transform(*cube).position = {0.0, 0.0, 0.0};
    if (!add_mesh(*cube, engine::scene::kBuiltinCubeMesh)) {
        return std::nullopt;
    }

    scene.transform(*ground).position = {0.0, -1.0, 0.0};
    if (!add_mesh(*ground, engine::scene::kBuiltinPlaneMesh)) {
        return std::nullopt;
    }

    return scene;
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
    window_options.title = "engine overlay test";
    window_options.width = 800;
    window_options.height = 600;
    window_options.resizable = false;
    window_options.hidden = true;
    window_options.high_pixel_density = false;

    auto window = engine::platform::Window::create(*sdl, window_options);
    if (!window) {
        return fail("could not create the window: " + window.error().message);
    }

    engine::render::DeviceOptions device_options;
    device_options.application_name = "engine overlay test";
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

    auto scene = build_test_scene();
    if (!scene) {
        return fail("could not build the test scene");
    }
    const engine::scene::SceneView scene_view(*scene);
    const std::uint64_t revision_before = scene->revision();
    const std::size_t entity_count_before = scene->size();
    std::optional<engine::scene::Entity> camera_entity;
    for (const engine::scene::Entity entity : scene->entities()) {
        if (scene->name(entity) == "Camera") {
            camera_entity = entity;
            break;
        }
    }
    if (!camera_entity.has_value()) {
        return fail("the test scene has no camera entity");
    }
    const auto camera_position_before = scene->transform(*camera_entity).position;

    engine::render::SwapchainOptions options;
    options.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    options.debug_overlay = true;

    auto renderer = engine::render::Renderer::create(*device, *window, options);
    if (!renderer) {
        return fail("could not create the renderer: " + renderer.error().message + " " +
                    renderer.error().hint);
    }
    if (!renderer->overlay_enabled()) {
        return fail("the renderer did not enable the debug overlay");
    }

    bool saw_stats = false;
    for (int frame = 0; frame < 4; ++frame) {
        auto drawn = renderer->draw_frame(scene_view);
        if (!drawn) {
            return fail("could not draw frame " + std::to_string(frame) + ": " +
                        drawn.error().message);
        }
        if (!*drawn) {
            return fail("frame " + std::to_string(frame) + " was not presented");
        }
        if (renderer->overlay_entity_count() != entity_count_before) {
            return fail("the overlay did not list every entity");
        }
        if (renderer->overlay_draw_vertices() == 0) {
            return fail("the overlay produced no vertices");
        }
        const engine::render::FrameStats stats = renderer->frame_stats();
        if (stats.frames != renderer->presented_frames()) {
            return fail("the frame statistics do not follow the presented frames");
        }
        if (stats.cpu_frame_ms <= 0.0) {
            return fail("the CPU frame time was not measured");
        }
        if (frame > 0 && stats.frames_per_second <= 0.0) {
            return fail("the frame rate was not measured");
        }
        if (stats.gpu_timing_supported && stats.gpu_frame_ms < 0.0) {
            return fail("the GPU frame time is invalid");
        }
        saw_stats = true;
    }
    if (!saw_stats) {
        return fail("no frame statistics were collected");
    }
    const engine::render::FrameStats final_stats = renderer->frame_stats();
    if (final_stats.gpu_timing_supported && final_stats.gpu_samples == 0) {
        return fail("the GPU timestamp path produced no samples");
    }

    if (!renderer->overlay_visible()) {
        return fail("the overlay was not visible initially");
    }
    renderer->set_overlay_visible(false);
    auto hidden = renderer->draw_frame(scene_view);
    if (!hidden || !*hidden) {
        return fail("could not draw with the overlay hidden");
    }
    if (renderer->overlay_visible() || renderer->overlay_draw_vertices() != 0) {
        return fail("the overlay stayed visible after being hidden");
    }
    renderer->set_overlay_visible(true);
    auto shown = renderer->draw_frame(scene_view);
    if (!shown || !*shown) {
        return fail("could not draw with the overlay shown again");
    }
    if (!renderer->overlay_visible() || renderer->overlay_draw_vertices() == 0) {
        return fail("the overlay did not return after being shown again");
    }

    if (scene->revision() != revision_before || scene->size() != entity_count_before) {
        return fail("the overlay mutated the scene");
    }
    const auto& camera_position_after = scene->transform(*camera_entity).position;
    if (camera_position_after.x != camera_position_before.x ||
        camera_position_after.y != camera_position_before.y ||
        camera_position_after.z != camera_position_before.z) {
        return fail("the overlay changed a transform");
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

    std::cout << "overlay rendered " << renderer->overlay_draw_vertices() << " vertices for "
              << renderer->overlay_entity_count() << " entities, CPU "
              << renderer->frame_stats().cpu_frame_ms << " ms, GPU "
              << renderer->frame_stats().gpu_frame_ms << " ms\n";
    return 0;
}
