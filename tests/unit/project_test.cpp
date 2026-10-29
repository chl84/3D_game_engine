#include <engine/scene/project.hpp>

#include <engine/platform/atomic_file.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

#include "temp_dir.hpp"

using engine::core::ReflectionRegistry;
using engine::core::Vec3;
using engine::scene::Camera;
using engine::scene::Entity;
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
    scene.transform(root).position = {1.0F, 2.0F, 3.0F};
    scene.transform(child).position = {0.0F, 5.0F, 0.0F};
    REQUIRE(scene.set_parent(child, root).has_value());
    REQUIRE(scene.add_component<Camera>(child).has_value());
    scene.component<Camera>(child).fovYRadians = 0.75F;
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
    REQUIRE(loaded->transform(loaded_root.value_or(entt::null)).position == Vec3{1.0F, 2.0F, 3.0F});
    REQUIRE(loaded->has<Camera>(loaded_child.value_or(entt::null)));
    REQUIRE(loaded->component<Camera>(loaded_child.value_or(entt::null)).fovYRadians == 0.75F);
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
    scene_document["formatVersion"] = 2;
    REQUIRE(engine::platform::atomic_write_file(project->root() / "scenes/main.scene.json",
                                                scene_document.dump(2) + "\n")
                .has_value());
    auto newer_scene = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE_FALSE(newer_scene.has_value());
    REQUIRE(newer_scene.error().code == engine::core::ErrorCode::Unsupported);
}

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
