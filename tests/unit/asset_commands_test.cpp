#include "temp_dir.hpp"
#include <catch2/catch_test_macros.hpp>
#include <engine/asset/asset_library.hpp>
#include <engine/asset/commands.hpp>
#include <engine/scene/project.hpp>
#include <engine/scene/scene.hpp>
#include <filesystem>
#include <limits>

TEST_CASE("asset commands share schemas, validation and command logging", "[asset][command]") {
    engine::test::TempDir temp("asset-command");
    auto project = engine::scene::Project::create(temp.path() / "project", "Commands");
    REQUIRE(project);
    engine::command::CommandLog log;
    engine::command::CommandBus bus(log);
    REQUIRE(engine::asset::register_asset_commands(bus, project->root(), true));
    CHECK(bus.registry().names().size() == 5U);
    auto bad = bus.execute({"asset.inspect@1", {{"assetId", "../outside"}}});
    REQUIRE_FALSE(bad.ok);
    REQUIRE(bad.error);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(bad.error->code == engine::core::ErrorCode::Validation);
    auto empty = bus.execute({"asset.list@1", nlohmann::json::object()});
    REQUIRE(empty.ok);
    CHECK(empty.data.at("assets").empty());
    REQUIRE(bus.begin_transaction());
    const auto source = std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / "blender_cube.glb";
    auto transaction = bus.execute({"asset.import@1", {{"source", source.string()}}});
    REQUIRE_FALSE(transaction.ok);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(transaction.error->code == engine::core::ErrorCode::Unsupported);
    REQUIRE(bus.rollback_transaction());
    auto preview = bus.execute({"asset.import@1", {{"source", source.string()}}});
    REQUIRE(preview.ok);
    CHECK(preview.data.at("dryRun") == true);
    CHECK_FALSE(std::filesystem::exists(project->root() / "assets" /
                                        preview.data.at("assetId").get<std::string>()));
    CHECK(log.entries().size() == 4U);
}

TEST_CASE("asset placement is validated, undoable and preserves instance references",
          "[asset][command][placement]") {
    using namespace engine;
    test::TempDir temp("asset-place");
    auto project = scene::Project::create(temp.path() / "project", "Placement");
    REQUIRE(project);
    auto imported = asset::import_asset(
        project->root(), std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / "blender_cube.glb");
    REQUIRE(imported);
    const auto asset_id = imported->record.asset_id.to_string();
    scene::Scene scene = scene::Scene::with_seed(71);
    command::CommandLog log;
    command::CommandBus bus(log);
    REQUIRE(asset::register_asset_scene_commands(bus, project->root(), scene));
    const auto revision = scene.revision();
    for (const auto& args : {
             nlohmann::json{{"assetId", "invalid"}},
             nlohmann::json{{"assetId", core::Uuid::generate().to_string()}},
             nlohmann::json{{"assetId", asset_id}, {"position", {1, 2}}},
             nlohmann::json{
                 {"assetId", asset_id},
                 {"position", {std::numeric_limits<double>::infinity(), 0, 0}},
             },
         }) {
        REQUIRE_FALSE(bus.execute({"asset.place@1", args}).ok);
        CHECK(scene.size() == 0);
        CHECK(scene.revision() == revision);
    }
    const nlohmann::json args = {
        {"assetId", asset_id},
        {"name", "Instance"},
        {"position", {16000.0001, 2.0, -3.0}},
    };
    auto first = bus.execute({"asset.place@1", args});
    REQUIRE(first.ok);
    scene::Scene replay = scene::Scene::with_seed(71);
    command::CommandLog replay_log;
    command::CommandBus replay_bus(replay_log);
    REQUIRE(asset::register_asset_scene_commands(replay_bus, project->root(), replay));
    auto replayed = replay_bus.execute({"asset.place@1", args});
    REQUIRE(replayed.ok);
    CHECK(replayed.data.at("id") == first.data.at("id"));
    const auto id = core::Uuid::parse(first.data.at("id").get<std::string>());
    REQUIRE(id);
    const auto entity = scene.find(*id);
    REQUIRE(entity);
    const auto placed_entity = entity.value_or(entt::null);
    CHECK(scene.transform(placed_entity).position.x == 16000.0001);
    CHECK(scene.component<scene::AssetInstance>(placed_entity).asset_id == asset_id);
    CHECK(scene.children(placed_entity).empty());
    REQUIRE(bus.undo());
    CHECK(scene.size() == 0);
    REQUIRE(bus.redo());
    CHECK(scene.find(*id).has_value());
    auto second = bus.execute({"asset.place@1", {{"assetId", asset_id}}});
    REQUIRE(second.ok);
    CHECK(second.data.at("id") != first.data.at("id"));
    CHECK(second.data.at("name") == imported->record.name);
    CHECK(scene.size() == 2);
    core::ReflectionRegistry registry;
    scene::register_scene_components(registry);
    REQUIRE(project->save_scene("scenes/main.scene.json", scene, registry));
    auto restored = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE(restored);
    CHECK(restored->size() == 2);
    for (const auto current : restored->entities()) {
        CHECK(restored->component<scene::AssetInstance>(current).asset_id == asset_id);
        CHECK(restored->children(current).empty());
    }
    REQUIRE(bus.begin_transaction());
    REQUIRE(bus.execute({"asset.place@1", args}).ok);
    REQUIRE_FALSE(bus.execute({"asset.place@1", {{"assetId", "invalid"}}}).ok);
    REQUIRE(bus.rollback_transaction());
    CHECK(scene.size() == 2);
}
