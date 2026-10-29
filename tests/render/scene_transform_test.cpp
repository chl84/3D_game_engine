#include <engine/render/camera.hpp>
#include <engine/render/frustum.hpp>
#include <engine/render/scene_transform.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cmath>

namespace {

[[nodiscard]] engine::scene::WorldTransform far_world() {
    engine::scene::WorldTransform world;
    world.position = engine::core::Vec3d{8000.001, 0.0, 7994.999};
    return world;
}

} // namespace

TEST_CASE("camera-relative model keeps 8 km precision", "[render][transform]") {
    const engine::scene::WorldTransform world = far_world();
    const engine::core::Vec3d camera_position{8000.0, 0.0, 8000.0};

    const glm::mat4 model = engine::render::camera_relative_model(world, camera_position);
    CHECK(model[3][0] == Catch::Approx(0.001F).margin(1e-6F));
    CHECK(model[3][1] == Catch::Approx(0.0F).margin(1e-6F));
    CHECK(model[3][2] == Catch::Approx(-5.001F).margin(1e-5F));

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            CHECK(std::isfinite(model[column][row]));
        }
    }
}

TEST_CASE("camera view at the origin ignores the world translation", "[render][transform]") {
    engine::scene::WorldTransform camera_world;
    camera_world.position = engine::core::Vec3d{8000.0, 2.0, 8000.0};

    const glm::mat4 view = engine::render::camera_view_from_world(camera_world);
    CHECK(view[3][0] == Catch::Approx(0.0F).margin(1e-6F));
    CHECK(view[3][1] == Catch::Approx(0.0F).margin(1e-6F));
    CHECK(view[3][2] == Catch::Approx(0.0F).margin(1e-6F));
}

TEST_CASE("frustum culls spheres behind and outside the camera", "[render][frustum]") {
    engine::render::Camera camera;
    camera.position = glm::vec3{0.0F, 0.0F, 0.0F};
    camera.target = glm::vec3{0.0F, 0.0F, -1.0F};
    camera.up = glm::vec3{0.0F, 1.0F, 0.0F};

    const glm::mat4 view_projection = camera.view_projection(16.0F / 9.0F);
    const engine::render::Frustum frustum =
        engine::render::Frustum::from_view_projection(view_projection);

    CHECK(frustum.intersects_sphere(glm::vec3{0.0F, 0.0F, -5.0F}, 0.5F));
    CHECK_FALSE(frustum.intersects_sphere(glm::vec3{0.0F, 0.0F, 20.0F}, 0.5F));
    CHECK_FALSE(frustum.intersects_sphere(glm::vec3{100.0F, 0.0F, -5.0F}, 0.5F));
}
