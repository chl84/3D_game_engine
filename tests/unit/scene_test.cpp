#include <engine/scene/scene.hpp>

#include <algorithm>
#include <cmath>
#include <ranges>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using engine::core::Quat;
using engine::core::ReflectionRegistry;
using engine::core::Uuid;
using engine::core::Vec3;
using engine::core::Vec3d;
using engine::scene::Entity;
using engine::scene::register_scene_components;
using engine::scene::Scene;

namespace {

[[nodiscard]] Entity require_entity(const engine::core::Result<Entity>& result) {
    REQUIRE(result.has_value());
    return *result;
}

} // namespace

TEST_CASE("scene generates deterministic UUIDs from a seed", "[scene]") {
    Scene first = Scene::with_seed(42);
    Scene second = Scene::with_seed(42);

    const Entity first_entity = require_entity(first.create_entity("Root"));
    const Entity second_entity = require_entity(second.create_entity("Root"));

    const auto first_uuid = first.uuid(first_entity);
    const auto second_uuid = second.uuid(second_entity);
    REQUIRE(first_uuid.has_value());
    REQUIRE(second_uuid.has_value());
    REQUIRE(first_uuid == second_uuid);
    REQUIRE(first_uuid.value_or(Uuid{}).to_string() == second_uuid.value_or(Uuid{}).to_string());
    REQUIRE(first_entity != entt::null);
}

TEST_CASE("scene rejects duplicate explicit UUIDs", "[scene]") {
    Scene scene = Scene::with_seed(1);
    const auto parsed = Uuid::parse("a1b2c3d4-4e5f-4a6b-8c7d-9e0f1a2b3c4d");
    REQUIRE(parsed.has_value());

    const auto first = scene.create_entity("First", *parsed);
    REQUIRE(first.has_value());

    const auto duplicate = scene.create_entity("Second", *parsed);
    REQUIRE_FALSE(duplicate.has_value());
    REQUIRE(duplicate.error().code == engine::core::ErrorCode::Conflict);
}

TEST_CASE("scene maintains parent and child relationships", "[scene]") {
    Scene scene = Scene::with_seed(2);

    const Entity parent = require_entity(scene.create_entity("Parent"));
    const Entity child = require_entity(scene.create_entity("Child"));

    auto result = scene.set_parent(child, parent);
    REQUIRE(result.has_value());
    REQUIRE(scene.parent(child) == parent);
    REQUIRE(scene.children(parent).size() == 1);
    REQUIRE(scene.children(parent).front() == child);

    result = scene.set_parent(child, entt::null);
    REQUIRE(result.has_value());
    REQUIRE(scene.parent(child) == entt::null);
    REQUIRE(scene.children(parent).empty());
}

TEST_CASE("scene computes world transforms through the hierarchy", "[scene]") {
    Scene scene = Scene::with_seed(3);

    const Entity parent = require_entity(scene.create_entity("Parent"));
    const Entity child = require_entity(scene.create_entity("Child"));

    scene.transform(parent).position = {10.0F, 0.0F, 0.0F};
    scene.transform(parent).scale = {2.0F, 2.0F, 2.0F};
    scene.transform(child).position = {1.0F, 5.0F, 0.0F};

    REQUIRE(scene.set_parent(child, parent).has_value());

    const auto world = scene.world_transform(child);
    REQUIRE(world.position == Vec3d{12.0, 10.0, 0.0});
    REQUIRE(world.scale == Vec3d{2.0, 2.0, 2.0});

    scene.transform(parent).rotation = Quat{0.0F, 1.0F, 0.0F, 0.0F};
    const auto rotated = scene.world_transform(child);
    REQUIRE(rotated.position.x == 8.0);
    REQUIRE(rotated.position.y == 10.0);
    REQUIRE(rotated.position.z == 0.0);
}

TEST_CASE("scene composes 8 km transforms in double precision", "[scene]") {
    Scene scene = Scene::with_seed(5);
    const Entity parent = require_entity(scene.create_entity("FarParent"));
    const Entity child = require_entity(scene.create_entity("FarChild"));

    scene.transform(parent).position = {8000.0F, 0.0F, 8000.0F};
    scene.transform(child).position = {0.001F, 0.0F, -5.001F};
    REQUIRE(scene.set_parent(child, parent).has_value());

    const auto world = scene.world_transform(child);
    CHECK(world.position.x == Catch::Approx(8000.001).margin(1e-9));
    CHECK(world.position.z == Catch::Approx(7994.999).margin(1e-9));

    const Vec3d camera_position{8000.0, 0.0, 8000.0};
    const Vec3d camera_relative = world.position - camera_position;
    const auto relative_x = static_cast<float>(camera_relative.x);
    const auto relative_z = static_cast<float>(camera_relative.z);
    CHECK(std::abs(relative_x - 0.001F) < 1e-6F);
    CHECK(std::abs(relative_z - (-5.001F)) < 1e-5F);
}

TEST_CASE("scene rejects hierarchy cycles", "[scene]") {
    Scene scene = Scene::with_seed(4);

    const Entity parent = require_entity(scene.create_entity("Parent"));
    const Entity child = require_entity(scene.create_entity("Child"));
    REQUIRE(scene.set_parent(child, parent).has_value());

    const auto cycle = scene.set_parent(parent, child);
    REQUIRE_FALSE(cycle.has_value());
    REQUIRE(cycle.error().code == engine::core::ErrorCode::Validation);

    const auto self = scene.set_parent(parent, parent);
    REQUIRE_FALSE(self.has_value());
    REQUIRE(self.error().code == engine::core::ErrorCode::Validation);
}

TEST_CASE("destroying an entity removes its subtree and UUIDs", "[scene]") {
    Scene scene = Scene::with_seed(5);

    const Entity parent = require_entity(scene.create_entity("Parent"));
    const Entity child = require_entity(scene.create_entity("Child"));
    REQUIRE(scene.set_parent(child, parent).has_value());

    const auto parent_id = scene.uuid(parent);
    const auto child_id = scene.uuid(child);
    REQUIRE(parent_id.has_value());
    REQUIRE(child_id.has_value());

    REQUIRE(scene.destroy_entity(parent).has_value());
    REQUIRE_FALSE(scene.contains(parent));
    REQUIRE_FALSE(scene.contains(child));
    REQUIRE_FALSE(scene.find(parent_id.value_or(Uuid{})).has_value());
    REQUIRE_FALSE(scene.find(child_id.value_or(Uuid{})).has_value());
    REQUIRE(scene.size() == 0);
}

TEST_CASE("scene lists entities in UUID order", "[scene]") {
    Scene scene = Scene::with_seed(6);

    const Entity first = require_entity(scene.create_entity("First"));
    const Entity second = require_entity(scene.create_entity("Second"));
    const Entity third = require_entity(scene.create_entity("Third"));

    const auto entities = scene.entities();
    REQUIRE(entities.size() == 3);
    REQUIRE(std::ranges::is_sorted(entities, [&scene](Entity left, Entity right) {
        return scene.uuid(left) < scene.uuid(right);
    }));
    REQUIRE(std::ranges::find(entities, first) != entities.end());
    REQUIRE(std::ranges::find(entities, second) != entities.end());
    REQUIRE(std::ranges::find(entities, third) != entities.end());
}

TEST_CASE("scene registers components in the reflection registry", "[scene][reflect]") {
    ReflectionRegistry registry;
    register_scene_components(registry);

    const auto* transform = registry.find("engine.scene.Transform");
    REQUIRE(transform != nullptr);
    REQUIRE(transform->fields().size() == 3);
    REQUIRE(transform->find_field("position") != nullptr);

    const auto* camera = registry.find("engine.scene.Camera");
    REQUIRE(camera != nullptr);
    REQUIRE(camera->fields().size() == 4);

    const auto schema = registry.schema("engine.scene.Transform");
    REQUIRE(schema.has_value());
    REQUIRE(schema->at("properties").contains("rotation"));
}
