#include <engine/scene/scene_view.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using engine::scene::Camera;
using engine::scene::Entity;
using engine::scene::Mesh;
using engine::scene::Scene;
using engine::scene::SceneView;

namespace {

[[nodiscard]] Entity require_entity(const engine::core::Result<Entity>& result) {
    REQUIRE(result.has_value());
    return *result;
}

} // namespace

TEST_CASE("scene view exposes meshes and cameras without mutation", "[scene][view]") {
    Scene scene = Scene::with_seed(12);
    const Entity mesh_entity = require_entity(scene.create_entity("Mesh"));
    const Entity camera_entity = require_entity(scene.create_entity("Camera"));

    REQUIRE(scene.add_component<Mesh>(mesh_entity).has_value());
    scene.component<Mesh>(mesh_entity).mesh_id = std::string(engine::scene::kBuiltinPlaneMesh);
    REQUIRE(scene.add_component<Camera>(camera_entity).has_value());
    scene.transform(camera_entity).position = {8000.0, 2.0, 8000.0};

    const SceneView view(scene);
    CHECK(view.contains(mesh_entity));
    CHECK(view.name(mesh_entity) == "Mesh");
    REQUIRE(view.has_mesh(mesh_entity));
    CHECK(view.mesh(mesh_entity).mesh_id == engine::scene::kBuiltinPlaneMesh);
    REQUIRE(view.has_camera(camera_entity));
    CHECK(view.camera(camera_entity).enabled);
    CHECK(view.active_camera() == camera_entity);
    CHECK(view.world_transform(camera_entity).position.x == 8000.0);

    scene.component<Camera>(camera_entity).enabled = false;
    CHECK_FALSE(view.active_camera().has_value());
}
