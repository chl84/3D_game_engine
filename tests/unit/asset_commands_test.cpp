#include "temp_dir.hpp"
#include <catch2/catch_test_macros.hpp>
#include <engine/asset/commands.hpp>
#include <engine/scene/project.hpp>
#include <filesystem>

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
