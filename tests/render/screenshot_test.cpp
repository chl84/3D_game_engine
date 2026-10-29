#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <engine/render/device.hpp>
#include <engine/render/offscreen_renderer.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/scene_view.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kSkipExitCode = 77;
constexpr int kWidth = 64;
constexpr int kHeight = 64;

int skip(const std::string& message) {
    std::cout << "SKIPPED: " << message << '\n';
    return kSkipExitCode;
}

int fail(const std::string& message) {
    std::cerr << "screenshot test failed: " << message << '\n';
    return 1;
}

[[nodiscard]] std::optional<engine::scene::Scene> build_scene() {
    using engine::scene::Camera;
    using engine::scene::Entity;
    using engine::scene::Mesh;
    using engine::scene::Scene;

    Scene scene = Scene::with_seed(23);
    auto camera = scene.create_entity("Camera");
    auto cube = scene.create_entity("Cube");
    auto ground = scene.create_entity("Ground");
    if (!camera || !cube || !ground) {
        return std::nullopt;
    }
    scene.transform(*camera).position = {0.0, 0.0, 4.0};
    if (!scene.add_component<Camera>(*camera)) {
        return std::nullopt;
    }
    scene.component<Camera>(*camera).fovYRadians = 1.0F;
    if (!scene.add_component<Mesh>(*cube)) {
        return std::nullopt;
    }
    scene.component<Mesh>(*cube).mesh_id = std::string(engine::scene::kBuiltinCubeMesh);
    if (!scene.add_component<Mesh>(*ground)) {
        return std::nullopt;
    }
    scene.component<Mesh>(*ground).mesh_id = std::string(engine::scene::kBuiltinPlaneMesh);
    scene.transform(*ground).position = {0.0, -0.75, 0.0};
    return scene;
}

} // namespace

int main(int argc, char** argv) {
    const std::string golden_path = argc > 1 ? argv[1] : "";
    const bool write_golden = argc > 2 && std::string(argv[2]) == "--write";
    if (golden_path.empty()) {
        return fail("missing golden image path");
    }

    auto scene = build_scene();
    if (!scene) {
        return fail("could not build the screenshot scene");
    }
    engine::scene::SceneView view(*scene);

    engine::render::DeviceOptions device_options;
    device_options.application_name = "engine screenshot test";
    device_options.validation = engine::render::ValidationMode::Auto;
    auto device = engine::render::Device::create(device_options);
    if (!device) {
        return skip("Vulkan device is unavailable: " + device.error().message);
    }
    if (device->device_name().find("llvmpipe") == std::string::npos &&
        device->device_name().find("lavapipe") == std::string::npos) {
        return skip("golden images require lavapipe; device is " + device->device_name());
    }

    engine::render::OffscreenOptions options;
    options.width = kWidth;
    options.height = kHeight;
    auto renderer = engine::render::OffscreenRenderer::create(*device, options);
    if (!renderer) {
        return fail("could not create the offscreen renderer: " + renderer.error().message);
    }
    auto pixels = renderer->render(view, 3);
    if (!pixels) {
        return fail("could not render the screenshot: " + pixels.error().message);
    }

    if (write_golden) {
        if (stbi_write_png(golden_path.c_str(), kWidth, kHeight, 4, pixels->data(), kWidth * 4) ==
            0) {
            return fail("could not write the golden image");
        }
        std::cout << "wrote golden image " << golden_path << '\n';
        return 0;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* loaded = stbi_load(golden_path.c_str(), &width, &height, &channels, 4);
    if (loaded == nullptr) {
        return fail("could not load the golden image: " + golden_path);
    }
    std::vector<std::uint8_t> golden(
        loaded, loaded + (static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U));
    stbi_image_free(loaded);
    if (width != kWidth || height != kHeight) {
        return fail("the golden image has the wrong size");
    }

    std::size_t differing = 0;
    int maximum_difference = 0;
    for (std::size_t index = 0; index < pixels->size(); ++index) {
        const int difference =
            std::abs(static_cast<int>((*pixels)[index]) - static_cast<int>(golden[index]));
        maximum_difference = std::max(maximum_difference, difference);
        if (difference > 2) {
            ++differing;
        }
    }
    if (maximum_difference > 8 || differing > (pixels->size() / 100)) {
        return fail("the rendered image differs from the golden image (max " +
                    std::to_string(maximum_difference) + ", differing channels " +
                    std::to_string(differing) + ")");
    }

    std::cout << "golden image matches (max channel difference " << maximum_difference << ")\n";
    return 0;
}
