#include "pbr_probes.hpp"
#include "temp_dir.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <engine/asset/asset_library.hpp>
#include <engine/render/offscreen_renderer.hpp>
#include <engine/render/renderer.hpp>
#include <engine/scene/project.hpp>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace engine;
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
template <class T> T take(core::Result<T>&& result) {
    if (!result) {
        throw std::runtime_error(result.error().message + " " + result.error().hint + " " +
                                 result.error().details.dump());
    }
    return std::move(*result);
}
void good(core::Result<void>&& result) {
    if (!result) {
        throw std::runtime_error(result.error().message + " " + result.error().details.dump());
    }
}
scene::Scene make_scene(const std::string& id) {
    scene::Scene s;
    auto camera = take(s.create_entity("Camera"));
    good(s.add_component<scene::Camera>(camera));
    s.component<scene::Camera>(camera).fovYRadians = 0.65F;
    s.transform(camera).position = {3.7, 4.6, 4.5};
    const glm::quat q =
        glm::quatLookAtRH(glm::normalize(glm::vec3{-3.7F, -4.6F, -4.5F}), glm::vec3{0, 1, 0});
    s.transform(camera).rotation = {q.x, q.y, q.z, q.w};
    auto model = take(s.create_entity("Model"));
    good(s.add_component<scene::AssetInstance>(model));
    s.component<scene::AssetInstance>(model).asset_id = id;
    auto light = take(s.create_entity("Sun"));
    good(s.add_component<scene::DirectionalLight>(light));
    auto& sun = s.component<scene::DirectionalLight>(light);
    sun.direction = {-0.4F, -1.0F, -0.3F};
    sun.intensity = 3;
    return s;
}
void set_asset(scene::Scene& s, const std::string& id) {
    for (auto e : s.entities()) {
        if (s.has<scene::AssetInstance>(e)) {
            s.component<scene::AssetInstance>(e).asset_id = id;
        }
    }
}
std::uint64_t difference(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
    require(a.size() == b.size(), "image size differs");
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum +=
            static_cast<std::uint64_t>(std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])));
    }
    return sum;
}
bool images_match(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    std::uint64_t changed = 0;
    int maximum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto delta = std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
        maximum = std::max(maximum, delta);
        if (delta > 3) {
            ++changed;
        }
    }
    return maximum <= 8 && changed < a.size() / 1000U;
}
asset::LoadedAsset load(const std::filesystem::path& root, const std::string& file) {
    auto imported =
        take(asset::import_asset(root, std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / file));
    return take(asset::load_asset(root, imported.record.asset_id));
}
void validation(const render::Device& device) {
    const auto report = device.validation_report();
    for (const auto& message : report.messages) {
        std::cerr << message << '\n';
    }
    require(report.errors == 0, "Vulkan validation errors");
}
} // namespace
int main(int argc, char** argv) {
    try {
        const bool window_test = argc > 1 && std::string(argv[1]) == "--window";
        const bool device_smoke = argc > 1 && std::string(argv[1]) == "--device-smoke";
        const bool color_test = argc > 2 && std::string(argv[1]) == "--color-golden";
        const bool write_golden = (argc > 2 && std::string(argv[2]) == "--write") ||
                                  (argc > 3 && std::string(argv[3]) == "--write");
        test::TempDir temp("pbr-golden");
        auto project = take(scene::Project::create(temp.path() / "project", "PBR"));
        auto generated = load(project.root(), "blender_mirrored_no_tangents.glb");
        auto exported = load(project.root(), "blender_mirrored_tangents.glb");
        auto scene = make_scene(generated.record.asset_id.to_string());
        std::unique_ptr<platform::SdlContext> sdl;
        std::unique_ptr<platform::Window> window;
        if (window_test) {
#ifdef __linux__
            if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) {
                std::cout << "SKIPPED: no display\n";
                return 77;
            }
#endif
            sdl = std::make_unique<platform::SdlContext>(take(platform::SdlContext::initialize()));
            platform::WindowOptions options;
            options.width = 320;
            options.height = 240;
            options.hidden = true;
            window =
                std::make_unique<platform::Window>(take(platform::Window::create(*sdl, options)));
        }
        render::DeviceOptions options;
        options.validation = render::ValidationMode::Required;
        options.window = window.get();
        auto device = take(render::Device::create(options));
        std::cout << "PBR device: " << device.device_name() << '\n';
        if (window_test) {
            render::SwapchainOptions swap;
            swap.debug_overlay = false;
            auto renderer = take(render::Renderer::create(device, *window, swap));
            auto missing = renderer.draw_frame(scene::SceneView(scene));
            require(!missing && missing.error().code == core::ErrorCode::NotFound &&
                        missing.error().details.at("assetId") ==
                            generated.record.asset_id.to_string(),
                    "missing upload did not report the scene asset ID");
            good(renderer.upload_asset(generated));
            good(renderer.upload_asset(exported));
            for (int frame = 0; frame < 2; ++frame) {
                require(take(renderer.draw_frame(scene::SceneView(scene))),
                        "window PBR did not present");
            }
            // Recoverable data errors must leave the current frame slot usable.
            auto extra_light = take(scene.create_entity("Extra sun"));
            good(scene.add_component<scene::DirectionalLight>(extra_light));
            auto denied_light = renderer.draw_frame(scene::SceneView(scene));
            require(!denied_light && denied_light.error().code == core::ErrorCode::Validation,
                    "multiple lights were accepted");
            good(scene.destroy_entity(extra_light));
            require(take(renderer.draw_frame(scene::SceneView(scene))),
                    "valid frame after light validation error did not present");
            scene::Scene no_camera;
            require(!renderer.draw_frame(scene::SceneView(no_camera)), "missing camera accepted");
            require(take(renderer.draw_frame(scene::SceneView(scene))),
                    "valid frame after missing camera did not present");
            auto before = renderer.pbr_stats();
            good(renderer.retire_asset(generated.record.asset_id.to_string()));
            require(renderer.pbr_stats().textures == before.textures &&
                        renderer.pbr_stats().retired_assets == 1,
                    "retired descriptor slots reused early");
            auto denied_asset = renderer.draw_frame(scene::SceneView(scene));
            require(!denied_asset && denied_asset.error().code == core::ErrorCode::NotFound,
                    "retired scene asset was accepted");
            set_asset(scene, exported.record.asset_id.to_string());
            for (int frame = 0; frame < 2; ++frame) {
                require(take(renderer.draw_frame(scene::SceneView(scene))),
                        "window reference did not present");
            }
            good(renderer.collect_assets());
            require(renderer.pbr_stats().textures < before.textures,
                    "completed slots not reclaimed");
            good(renderer.upload_asset(generated));
            good(renderer.wait_idle());
            validation(device);
            std::cout << "PBR error recovery, two-frame retirement and reuse passed\n";
            return 0;
        }
        require(argc > 1, "golden PNG path required");
        if (!device_smoke && device.device_name().find("llvmpipe") == std::string::npos &&
            device.device_name().find("lavapipe") == std::string::npos) {
            std::cout << "SKIPPED: golden requires lavapipe\n";
            return 77;
        }
        if (color_test) {
            const auto pixels = test::render_pbr_color_chart(device);
            if (write_golden) {
                require(stbi_write_png(argv[2], 192, 128, 4, pixels.data(), 192 * 4) != 0,
                        "cannot write color-space golden");
            } else {
                int width = 0;
                int height = 0;
                int channels = 0;
                auto* image = stbi_load(argv[2], &width, &height, &channels, 4);
                require(image != nullptr, "cannot load color-space golden");
                if (width != 192 || height != 128) {
                    stbi_image_free(image);
                    throw std::runtime_error("wrong color-space golden dimensions");
                }
                const std::vector<std::uint8_t> expected(image, image + pixels.size());
                stbi_image_free(image);
                require(images_match(pixels, expected), "color-space golden mismatch");
            }
            validation(device);
            std::cout << "PBR color-space fixed golden passed (sRGB base; linear "
                         "MR/AO/emissive/normal)\n";
            return 0;
        }
        auto renderer = take(render::OffscreenRenderer::create(device, {256, 192}));
        good(renderer.upload_asset(generated));
        good(renderer.upload_asset(exported));
        const auto original_revision = scene.revision();
        auto actual = take(renderer.render(scene::SceneView(scene), 2));
        set_asset(scene, exported.record.asset_id.to_string());
        auto reference = take(renderer.render(scene::SceneView(scene), 1));
        require(images_match(actual, reference),
                "generated tangents disagree with Blender reference image");
        std::cout << "Blender tangent image difference: " << difference(actual, reference) << '\n';
        // The fixture must be sensitive to both omitted normal maps and wrong handedness.
        auto flat = generated;
        flat.record.asset_id = core::Uuid::generate();
        for (auto& m : flat.asset.materials) {
            m.normal_scale = 0;
        }
        good(renderer.upload_asset(flat));
        set_asset(scene, flat.record.asset_id.to_string());
        auto flat_pixels = take(renderer.render(scene::SceneView(scene), 1));
        std::cerr << "normal difference: " << difference(actual, flat_pixels)
                  << " roots: " << generated.asset.scene_roots.size()
                  << " meshes: " << generated.asset.meshes.size() << '\n';
        require(!images_match(flat_pixels, reference),
                "Blender-reference tolerance accepts a missing normal map");
        require(difference(actual, flat_pixels) > 20000U,
                "normal map has no measurable image effect");
        auto flipped = generated;
        flipped.record.asset_id = core::Uuid::generate();
        for (auto& mesh : flipped.asset.meshes) {
            for (auto& primitive : mesh.primitives) {
                for (auto& v : primitive.vertices) {
                    v.tangent[3] *= -1;
                }
            }
        }
        good(renderer.upload_asset(flipped));
        set_asset(scene, flipped.record.asset_id.to_string());
        auto wrong = take(renderer.render(scene::SceneView(scene), 1));
        require(difference(actual, wrong) > 20000U, "golden insensitive to tangent handedness");
        // Camera-relative precision must preserve the exact image at 16 km.
        set_asset(scene, generated.record.asset_id.to_string());
        for (auto e : scene.entities()) {
            if (scene.has<scene::Camera>(e) || scene.has<scene::AssetInstance>(e)) {
                scene.transform(e).position.x += 16000;
            }
        }
        auto far = take(renderer.render(scene::SceneView(scene), 1));
        require(difference(actual, far) == 0, "16 km PBR precision regression");
        // Reflection changes winding and tangent handedness together.
        for (auto e : scene.entities()) {
            if (scene.has<scene::AssetInstance>(e)) {
                scene.transform(e).scale.x = -1;
            }
        }
        auto reflected = take(renderer.render(scene::SceneView(scene), 1));
        set_asset(scene, exported.record.asset_id.to_string());
        auto reflected_reference = take(renderer.render(scene::SceneView(scene), 1));
        require(images_match(reflected, reflected_reference),
                "reflected tangents disagree with Blender reference image");
        require(difference(reflected, actual) > 10000U, "reflection probe did not change image");
        require(scene.revision() == original_revision, "rendering mutated scene revision");
        require(!device.reserve_gpu_overhead_bytes(render::kTotalGpuMemoryBudgetBytes + 1U),
                "total GPU budget not enforced");
        require(!device.charge_texture_bytes(render::kTextureMemoryBudgetBytes + 1U),
                "texture budget not enforced");
        auto oversized = generated;
        oversized.record.asset_id = core::Uuid::generate();
        oversized.asset.materials.resize(4097);
        require(!renderer.upload_asset(oversized), "material slot overflow accepted");
        oversized = std::move(generated);
        oversized.record.asset_id = core::Uuid::generate();
        oversized.textures.resize(4097);
        require(!renderer.upload_asset(oversized), "texture slot overflow accepted");
        if (write_golden) {
            require(stbi_write_png(argv[1], 256, 192, 4, actual.data(), 256 * 4) != 0,
                    "cannot write golden");
        } else if (!device_smoke) {
            int width = 0;
            int height = 0;
            int channels = 0;
            auto* image = stbi_load(argv[1], &width, &height, &channels, 4);
            require(image != nullptr, "cannot read PBR golden");
            std::vector<std::uint8_t> golden(
                image,
                image + (static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U));
            stbi_image_free(image);
            require(width == 256 && height == 192, "wrong golden size");
            require(images_match(actual, golden), "PBR golden mismatch");
        }
        test::verify_pbr_probes(device);
        validation(device);
        std::cout << (device_smoke ? "PBR physical-device controls (fixed golden not compared)"
                                   : "PBR Blender golden")
                  << ", normal-map sensitivity, reflection, budgets and 16 km passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "PBR test failed: " << e.what() << '\n';
        return 1;
    }
}
