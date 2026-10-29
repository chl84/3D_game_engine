#include <engine/scene/scene.hpp>
#include <engine/scene/simulation.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using engine::core::Vec3;
using engine::scene::Entity;
using engine::scene::Motion;
using engine::scene::Scene;

namespace {

[[nodiscard]] Entity require_entity(const engine::core::Result<Entity>& result) {
    REQUIRE(result.has_value());
    return *result;
}

[[nodiscard]] Scene make_scene() {
    Scene scene = Scene::with_seed(99);
    const Entity first = require_entity(scene.create_entity("First"));
    const Entity second = require_entity(scene.create_entity("Second"));
    REQUIRE(scene.add_component<Motion>(first).has_value());
    REQUIRE(scene.add_component<Motion>(second).has_value());
    scene.component<Motion>(first).linearVelocity = {1.0F, 0.0F, 0.0F};
    scene.component<Motion>(second).linearVelocity = {0.0F, -2.0F, 0.0F};
    return scene;
}

} // namespace

TEST_CASE("fixed-tick simulation advances motion deterministically", "[simulation]") {
    Scene scene = make_scene();
    const auto dump = engine::scene::simulate(scene, 60, 60);

    REQUIRE(dump.at("ticks") == 60);
    REQUIRE(dump.at("tickRate") == 60);
    const auto& entities = dump.at("entities");
    REQUIRE(entities.size() == 2);
    for (const auto& entity : entities) {
        const auto& position = entity.at("transform").at("position");
        if (entity.at("name") == "First") {
            REQUIRE(position.at(0).get<float>() == Catch::Approx(1.0F));
            REQUIRE(position.at(1).get<float>() == Catch::Approx(0.0F));
            REQUIRE(position.at(2).get<float>() == Catch::Approx(0.0F));
        } else {
            REQUIRE(position.at(0).get<float>() == Catch::Approx(0.0F));
            REQUIRE(position.at(1).get<float>() == Catch::Approx(-2.0F));
            REQUIRE(position.at(2).get<float>() == Catch::Approx(0.0F));
        }
    }
}

TEST_CASE("two identical simulation runs produce identical dumps", "[simulation]") {
    Scene first = make_scene();
    Scene second = make_scene();

    const auto first_dump = engine::scene::simulate(first, 120, 60);
    const auto second_dump = engine::scene::simulate(second, 120, 60);

    REQUIRE(first_dump == second_dump);
    REQUIRE(first.scene_id() == second.scene_id());
}

TEST_CASE("entities without Motion stay still", "[simulation]") {
    Scene scene = Scene::with_seed(7);
    const Entity entity = require_entity(scene.create_entity("Static"));
    scene.transform(entity).position = {3.0, 4.0, 5.0};

    const auto dump = engine::scene::simulate(scene, 10, 60);
    REQUIRE(dump.at("entities").at(0).at("transform").at("position") ==
            nlohmann::json::array({3.0F, 4.0F, 5.0F}));
}
