#include <engine/scene/project.hpp>

#include <engine/platform/atomic_file.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

#include "temp_dir.hpp"

using engine::core::ReflectionRegistry;
using engine::core::Vec3d;
using engine::scene::AssetInstance;
using engine::scene::Camera;
using engine::scene::DirectionalLight;
using engine::scene::Entity;
using engine::scene::Mesh;
using engine::scene::PointLight;
using engine::scene::Project;
using engine::scene::register_scene_components;
using engine::scene::Scene;

namespace {

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

[[nodiscard]] Entity require_entity(const engine::core::Result<Entity>& result) {
    REQUIRE(result.has_value());
    return *result;
}

} // namespace

TEST_CASE("project create writes a canonical project and reloads it", "[project]") {
    engine::test::TempDir temp("project");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());
    REQUIRE(project->name() == "Demo");
    REQUIRE(project->scenes() == std::vector<std::string>{"scenes/main.scene.json"});

    const auto project_file = project->root() / "project.json";
    const std::string before = read_file(project_file);

    auto loaded = Project::load(project->root());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->project_id() == project->project_id());
    REQUIRE(loaded->save().has_value());
    REQUIRE(read_file(project_file) == before);
}

TEST_CASE("scene format round-trips entities, hierarchy and components", "[project][scene]") {
    engine::test::TempDir temp("scene");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    ReflectionRegistry registry;
    register_scene_components(registry);

    Scene scene = Scene::with_seed(42);
    scene.set_scene_name("main");
    const Entity root = require_entity(scene.create_entity("Root"));
    const Entity child = require_entity(scene.create_entity("Child"));
    scene.transform(root).position = {1.0, 2.0, 3.0};
    scene.transform(child).position = {0.0, 5.0, 0.0};
    REQUIRE(scene.set_parent(child, root).has_value());
    REQUIRE(scene.add_component<Camera>(child).has_value());
    scene.component<Camera>(child).fovYRadians = 0.75F;
    REQUIRE(scene.add_component<Mesh>(child).has_value());
    scene.component<Mesh>(child).mesh_id = std::string(engine::scene::kBuiltinPlaneMesh);
    REQUIRE(scene.add_component<AssetInstance>(child).has_value());
    scene.component<AssetInstance>(child).asset_id = "5f0c2a2e-7d4b-4c1e-9a2f-1f3d4c5b6a70";
    REQUIRE(scene.add_component<DirectionalLight>(root).has_value());
    scene.component<DirectionalLight>(root).direction = {0.0F, -1.0F, -0.5F};
    scene.component<DirectionalLight>(root).intensity = 2.5F;
    REQUIRE(scene.add_component<PointLight>(child).has_value());
    scene.component<PointLight>(child).range = 25.0F;
    REQUIRE(scene.set_metadata(child, {{"tag", "player"}}).has_value());

    const std::string scene_path = "scenes/main.scene.json";
    REQUIRE(project->save_scene(scene_path, scene, registry).has_value());
    const std::string before = read_file(project->root() / scene_path);

    auto loaded = project->load_scene(scene_path, registry);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->scene_id() == scene.scene_id());
    REQUIRE(loaded->scene_name() == "main");
    REQUIRE(loaded->revision() == scene.revision());
    REQUIRE(loaded->size() == 2);

    const auto loaded_root = loaded->find(scene.uuid(root).value_or(engine::core::Uuid{}));
    const auto loaded_child = loaded->find(scene.uuid(child).value_or(engine::core::Uuid{}));
    REQUIRE(loaded_root.has_value());
    REQUIRE(loaded_child.has_value());
    REQUIRE(loaded->parent(loaded_child.value_or(entt::null)) == loaded_root.value_or(entt::null));
    REQUIRE(loaded->transform(loaded_root.value_or(entt::null)).position == Vec3d{1.0, 2.0, 3.0});
    REQUIRE(loaded->has<Camera>(loaded_child.value_or(entt::null)));
    REQUIRE(loaded->component<Camera>(loaded_child.value_or(entt::null)).fovYRadians == 0.75F);
    REQUIRE(loaded->has<Mesh>(loaded_child.value_or(entt::null)));
    REQUIRE(loaded->component<Mesh>(loaded_child.value_or(entt::null)).mesh_id ==
            engine::scene::kBuiltinPlaneMesh);
    REQUIRE(loaded->component<AssetInstance>(loaded_child.value_or(entt::null)).asset_id ==
            "5f0c2a2e-7d4b-4c1e-9a2f-1f3d4c5b6a70");
    REQUIRE(loaded->component<DirectionalLight>(loaded_root.value_or(entt::null)).intensity ==
            2.5F);
    REQUIRE(loaded->component<PointLight>(loaded_child.value_or(entt::null)).range == 25.0F);
    REQUIRE(loaded->metadata(loaded_child.value_or(entt::null)).at("tag") == "player");

    REQUIRE(project->save_scene(scene_path, *loaded, registry).has_value());
    REQUIRE(read_file(project->root() / scene_path) == before);
}

TEST_CASE("project and scene loaders reject unknown fields and newer versions", "[project]") {
    engine::test::TempDir temp("invalid");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    auto project_document = nlohmann::json::parse(read_file(project->root() / "project.json"));
    project_document["unexpected"] = true;
    REQUIRE(engine::platform::atomic_write_file(project->root() / "project.json",
                                                project_document.dump(2) + "\n")
                .has_value());
    auto invalid_project = Project::load(project->root());
    REQUIRE_FALSE(invalid_project.has_value());
    REQUIRE(invalid_project.error().code == engine::core::ErrorCode::Validation);

    project_document.erase("unexpected");
    project_document["formatVersion"] = 2;
    REQUIRE(engine::platform::atomic_write_file(project->root() / "project.json",
                                                project_document.dump(2) + "\n")
                .has_value());
    auto newer_project = Project::load(project->root());
    REQUIRE_FALSE(newer_project.has_value());
    REQUIRE(newer_project.error().code == engine::core::ErrorCode::Unsupported);

    ReflectionRegistry registry;
    register_scene_components(registry);
    auto scene_document =
        nlohmann::json::parse(read_file(project->root() / "scenes/main.scene.json"));
    scene_document["formatVersion"] = 4;
    REQUIRE(engine::platform::atomic_write_file(project->root() / "scenes/main.scene.json",
                                                scene_document.dump(2) + "\n")
                .has_value());
    auto newer_scene = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(newer_scene.has_value());
    REQUIRE(newer_scene.error().code == engine::core::ErrorCode::Unsupported);
}

TEST_CASE("scene format v1 and v2 load and upgrade to v3", "[project][scene]") {
    engine::test::TempDir temp("scene-v1");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    ReflectionRegistry registry;
    register_scene_components(registry);
    const auto scene_file = project->root() / "scenes/main.scene.json";
    for (const int source_version : {1, 2}) {
        auto scene_document = nlohmann::json::parse(read_file(scene_file));
        REQUIRE(scene_document["formatVersion"] == 3);
        scene_document["formatVersion"] = source_version;
        REQUIRE(engine::platform::atomic_write_file(scene_file, scene_document.dump(2) + "\n")
                    .has_value());

        auto upgraded_scene = project->load_scene("scenes/main.scene.json", registry);
        REQUIRE(upgraded_scene.has_value());
        REQUIRE(
            project->save_scene("scenes/main.scene.json", *upgraded_scene, registry).has_value());
        const auto upgraded = nlohmann::json::parse(read_file(scene_file));
        REQUIRE(upgraded["formatVersion"] == 3);
    }

    auto loaded = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE(loaded.has_value());
    const Entity entity = require_entity(loaded->create_entity("Mesh"));
    loaded->transform(entity).position = {16000.001, 0.0, 8000.0};
    REQUIRE(loaded->add_component<Mesh>(entity).has_value());
    REQUIRE(project->save_scene("scenes/main.scene.json", *loaded, registry).has_value());
    auto mesh_document = nlohmann::json::parse(read_file(scene_file));
    REQUIRE(mesh_document["formatVersion"] == 3);
    REQUIRE(mesh_document["entities"][0]["components"]["engine.scene.Mesh"]["meshId"]
                .get<std::string>() == std::string(engine::scene::kBuiltinCubeMesh));
    REQUIRE(mesh_document["entities"][0]["components"]["engine.scene.Transform"]["position"][0]
                .get<double>() == 16000.001);

    auto reloaded = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE(reloaded.has_value());
    REQUIRE(loaded->uuid(entity).has_value());
    const auto entity_id = loaded->uuid(entity).value_or(engine::core::Uuid{});
    const auto reloaded_entity = reloaded->find(entity_id).value_or(entt::null);
    REQUIRE(reloaded->contains(reloaded_entity));
    REQUIRE(reloaded->transform(reloaded_entity).position == Vec3d{16000.001, 0.0, 8000.0});

    mesh_document["formatVersion"] = 1;
    REQUIRE(
        engine::platform::atomic_write_file(scene_file, mesh_document.dump(2) + "\n").has_value());
    auto invalid = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(invalid.has_value());
    REQUIRE(invalid.error().code == engine::core::ErrorCode::Validation);
}

TEST_CASE("scene format v3 components require version 3", "[project][scene]") {
    engine::test::TempDir temp("scene-v3-components");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    ReflectionRegistry registry;
    register_scene_components(registry);
    const auto scene_file = project->root() / "scenes/main.scene.json";
    auto scene_document = nlohmann::json::parse(read_file(scene_file));
    REQUIRE(scene_document["formatVersion"] == 3);
    auto& entity = scene_document["entities"][0];
    entity["components"]["engine.scene.DirectionalLight"] = {
        {"direction", {0.0, -1.0, 0.0}},
        {"color", {1.0, 1.0, 1.0}},
        {"intensity", 1.0},
        {"enabled", true},
    };
    scene_document["formatVersion"] = 2;
    REQUIRE(
        engine::platform::atomic_write_file(scene_file, scene_document.dump(2) + "\n").has_value());
    auto invalid = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(invalid.has_value());
    REQUIRE(invalid.error().code == engine::core::ErrorCode::Validation);
}

TEST_CASE("scene loader rejects missing IDs and negative revisions", "[project]") {
    engine::test::TempDir temp("invalid-scene");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    ReflectionRegistry registry;
    register_scene_components(registry);
    const auto scene_file = project->root() / "scenes" / "main.scene.json";
    const auto original = nlohmann::json::parse(read_file(scene_file));

    auto missing_id = original;
    missing_id.erase("sceneId");
    REQUIRE(engine::platform::atomic_write_file(scene_file, missing_id.dump(2) + "\n").has_value());
    auto invalid = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(invalid.has_value());
    REQUIRE(invalid.error().code == engine::core::ErrorCode::Validation);

    auto negative_revision = original;
    negative_revision["revision"] = -1;
    REQUIRE(engine::platform::atomic_write_file(scene_file, negative_revision.dump(2) + "\n")
                .has_value());
    auto negative = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(negative.has_value());
    REQUIRE(negative.error().code == engine::core::ErrorCode::Validation);
}

#ifndef _WIN32
TEST_CASE("atomic writes preserve existing file permissions", "[project]") {
    engine::test::TempDir temp("atomic-permissions");
    const auto target = temp.path() / "permissions.txt";

    REQUIRE(engine::platform::atomic_write_file(target, "first").has_value());
    const auto permissions = std::filesystem::perms::owner_read |
                             std::filesystem::perms::owner_write |
                             std::filesystem::perms::group_read;
    std::error_code error;
    std::filesystem::permissions(target, permissions, std::filesystem::perm_options::replace,
                                 error);
    REQUIRE_FALSE(error);

    REQUIRE(engine::platform::atomic_write_file(target, "second").has_value());
    REQUIRE(std::filesystem::status(target, error).permissions() == permissions);
    REQUIRE_FALSE(error);
}

TEST_CASE("atomic writes do not follow a pre-existing temporary symlink", "[project]") {
    engine::test::TempDir temp("atomic-symlink");
    const auto outside = temp.path() / "outside.txt";
    const auto target = temp.path() / "target.txt";
    const auto temporary = temp.path() / "target.txt.tmp";

    REQUIRE(engine::platform::atomic_write_file(outside, "safe").has_value());
    std::error_code error;
    std::filesystem::create_symlink(outside, temporary, error);
    REQUIRE_FALSE(error);

    REQUIRE(engine::platform::atomic_write_file(target, "new").has_value());
    REQUIRE(read_file(outside) == "safe");
    REQUIRE(read_file(target) == "new");
}
#endif

TEST_CASE("project paths cannot escape the project root", "[project]") {
    engine::test::TempDir temp("paths");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    ReflectionRegistry registry;
    register_scene_components(registry);
    Scene scene = Scene::with_seed(1);
    const auto result = project->save_scene("../escape.scene.json", scene, registry);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == engine::core::ErrorCode::Validation);
}

TEST_CASE("project lock is exclusive and can be released", "[project]") {
    engine::test::TempDir temp("lock");
    auto project = Project::create(temp.path() / "demo", "Demo");
    REQUIRE(project.has_value());

    REQUIRE(project->lock().has_value());
    REQUIRE(project->locked());

    auto second = Project::load(project->root());
    REQUIRE(second.has_value());
    const auto blocked = second->lock();
    REQUIRE_FALSE(blocked.has_value());
    REQUIRE(blocked.error().code == engine::core::ErrorCode::Conflict);

    project->unlock();
    REQUIRE_FALSE(project->locked());
    REQUIRE(second->lock().has_value());
}
