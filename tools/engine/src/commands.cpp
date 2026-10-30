#include "commands.hpp"
#include "scene_assets.hpp"
#include "scene_warnings.hpp"

#include <engine/asset/commands.hpp>
#include <engine/command/command.hpp>
#include <engine/core/path_sandbox.hpp>
#include <engine/render/device.hpp>
#include <engine/render/offscreen_renderer.hpp>
#include <engine/scene/project.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/scene_view.hpp>
#include <engine/scene/simulation.hpp>

#include <stb_image_write.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace engine::cli {
namespace {

using command::CommandBus;
using command::CommandLog;
using command::CommandOutput;
using command::CommandRequest;
using command::CommandResult;
using core::Error;
using core::ErrorCode;
using core::ReflectionRegistry;
using core::Result;
using core::Uuid;
using scene::AssetInstance;
using scene::Camera;
using scene::DirectionalLight;
using scene::Entity;
using scene::Mesh;
using scene::Motion;
using scene::PointLight;
using scene::Project;
using scene::Scene;
using scene::Transform;

[[nodiscard]] CommandResult success(nlohmann::json data, std::vector<std::string> warnings = {}) {
    return CommandResult::success(CommandOutput{std::move(data), std::move(warnings)});
}

[[nodiscard]] CommandResult failure(Error error) {
    return CommandResult::failure(std::move(error));
}

void print_result(const CommandResult& result, bool json) {
    if (json) {
        std::cout << result.to_json().dump(2) << '\n';
        return;
    }
    if (result.ok) {
        std::cout << result.data.dump(2) << '\n';
    } else if (result.error.has_value()) {
        std::cerr << "error: " << result.error->message << '\n';
        if (!result.error->hint.empty()) {
            std::cerr << "hint: " << result.error->hint << '\n';
        }
    }
    for (const std::string& warning : result.warnings) {
        std::cerr << "warning: " << warning << '\n';
    }
}

[[nodiscard]] int exit_code_for(const CommandResult& result) {
    if (result.ok) {
        return 0;
    }
    return result.error.has_value() ? core::exit_code(result.error->code) : 1;
}

[[nodiscard]] ReflectionRegistry make_registry() {
    ReflectionRegistry registry;
    scene::register_scene_components(registry);
    return registry;
}

[[nodiscard]] Result<Uuid> parse_uuid_argument(const nlohmann::json& arguments,
                                               std::string_view key) {
    const auto iterator = arguments.find(std::string(key));
    if (iterator == arguments.end() || !iterator->is_string()) {
        return core::make_failure<Uuid>(core::make_error(
            ErrorCode::Validation, "expected a UUID string", {}, {{"field", std::string(key)}}));
    }
    return Uuid::parse(iterator->get<std::string>());
}

[[nodiscard]] Result<std::optional<Uuid>> parse_parent_argument(const nlohmann::json& arguments) {
    const auto iterator = arguments.find("parentId");
    if (iterator == arguments.end() || iterator->is_null()) {
        return std::optional<Uuid>{};
    }
    if (!iterator->is_string()) {
        return core::make_failure<std::optional<Uuid>>(
            core::make_error(ErrorCode::Validation, "parentId must be a UUID string or null"));
    }
    auto parsed = Uuid::parse(iterator->get<std::string>());
    if (!parsed) {
        return core::make_failure<std::optional<Uuid>>(parsed.error());
    }
    return std::optional<Uuid>{*parsed};
}

[[nodiscard]] Result<Entity> find_entity(const Scene& scene, const nlohmann::json& arguments) {
    auto id = parse_uuid_argument(arguments, "id");
    if (!id) {
        return core::make_failure<Entity>(id.error());
    }
    const auto entity = scene.find(*id);
    if (!entity.has_value()) {
        return core::make_failure<Entity>(core::make_error(
            ErrorCode::NotFound, "entity does not exist", {}, {{"id", id->to_string()}}));
    }
    return *entity;
}

[[nodiscard]] Result<Entity> find_parent(const Scene& scene, const std::optional<Uuid>& parent) {
    if (!parent.has_value()) {
        return Entity{entt::null};
    }
    const auto entity = scene.find(*parent);
    if (!entity.has_value()) {
        return core::make_failure<Entity>(core::make_error(ErrorCode::NotFound,
                                                           "parent entity does not exist", {},
                                                           {{"parentId", parent->to_string()}}));
    }
    return *entity;
}

[[nodiscard]] Result<void*> mutable_component(Scene& scene, Entity entity,
                                              const std::string& component_name,
                                              bool& was_present) {
    was_present = true;
    if (component_name == "engine.scene.Transform") {
        return static_cast<void*>(&scene.transform(entity));
    }
    if (component_name == "engine.scene.Camera") {
        was_present = scene.has<Camera>(entity);
        if (!was_present) {
            auto added = scene.add_component<Camera>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<Camera>(entity));
    }
    if (component_name == "engine.scene.Motion") {
        was_present = scene.has<Motion>(entity);
        if (!was_present) {
            auto added = scene.add_component<Motion>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<Motion>(entity));
    }
    if (component_name == "engine.scene.Mesh") {
        was_present = scene.has<Mesh>(entity);
        if (!was_present) {
            auto added = scene.add_component<Mesh>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<Mesh>(entity));
    }
    if (component_name == "engine.scene.AssetInstance") {
        was_present = scene.has<AssetInstance>(entity);
        if (!was_present) {
            auto added = scene.add_component<AssetInstance>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<AssetInstance>(entity));
    }
    if (component_name == "engine.scene.DirectionalLight") {
        was_present = scene.has<DirectionalLight>(entity);
        if (!was_present) {
            auto added = scene.add_component<DirectionalLight>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<DirectionalLight>(entity));
    }
    if (component_name == "engine.scene.PointLight") {
        was_present = scene.has<PointLight>(entity);
        if (!was_present) {
            auto added = scene.add_component<PointLight>(entity);
            if (!added) {
                return core::make_failure<void*>(added.error());
            }
        }
        return static_cast<void*>(&scene.component<PointLight>(entity));
    }
    return core::make_failure<void*>(core::make_error(ErrorCode::NotFound, "unsupported component",
                                                      {}, {{"component", component_name}}));
}

[[nodiscard]] bool has_component_by_name(const Scene& scene, Entity entity,
                                         const std::string& component_name) {
    if (component_name == "engine.scene.Transform") {
        return true;
    }
    if (component_name == "engine.scene.Camera") {
        return scene.has<Camera>(entity);
    }
    if (component_name == "engine.scene.Motion") {
        return scene.has<Motion>(entity);
    }
    if (component_name == "engine.scene.Mesh") {
        return scene.has<Mesh>(entity);
    }
    if (component_name == "engine.scene.AssetInstance") {
        return scene.has<AssetInstance>(entity);
    }
    if (component_name == "engine.scene.DirectionalLight") {
        return scene.has<DirectionalLight>(entity);
    }
    if (component_name == "engine.scene.PointLight") {
        return scene.has<PointLight>(entity);
    }
    return false;
}

[[nodiscard]] Result<void> remove_component_by_name(Scene& scene, Entity entity,
                                                    const std::string& component_name) {
    if (component_name == "engine.scene.Camera") {
        return scene.remove_component<Camera>(entity);
    }
    if (component_name == "engine.scene.Motion") {
        return scene.remove_component<Motion>(entity);
    }
    if (component_name == "engine.scene.Mesh") {
        return scene.remove_component<Mesh>(entity);
    }
    if (component_name == "engine.scene.AssetInstance") {
        return scene.remove_component<AssetInstance>(entity);
    }
    if (component_name == "engine.scene.DirectionalLight") {
        return scene.remove_component<DirectionalLight>(entity);
    }
    if (component_name == "engine.scene.PointLight") {
        return scene.remove_component<PointLight>(entity);
    }
    return core::make_failure<void>(core::make_error(
        ErrorCode::Validation, "component cannot be removed", {}, {{"component", component_name}}));
}

[[nodiscard]] nlohmann::json entity_snapshot(const Scene& scene, const ReflectionRegistry& registry,
                                             Entity entity) {
    const auto* transform = registry.find("engine.scene.Transform");
    const auto* camera = registry.find("engine.scene.Camera");
    const auto id = scene.uuid(entity);

    nlohmann::json snapshot{
        {"id", id.value_or(Uuid{}).to_string()},
        {"name", std::string(scene.name(entity))},
        {"parentId", nullptr},
        {"components", nlohmann::json::object()},
        {"metadata", scene.metadata(entity)},
    };
    if (const Entity parent = scene.parent(entity); parent != entt::null) {
        if (const auto parent_id = scene.uuid(parent); parent_id.has_value()) {
            snapshot["parentId"] = parent_id->to_string();
        }
    }
    if (transform != nullptr) {
        snapshot["components"]["engine.scene.Transform"] =
            transform->to_json(&scene.transform(entity));
    }
    if (camera != nullptr && scene.has<Camera>(entity)) {
        snapshot["components"]["engine.scene.Camera"] =
            camera->to_json(&scene.component<Camera>(entity));
    }
    if (const auto* motion = registry.find("engine.scene.Motion");
        motion != nullptr && scene.has<Motion>(entity)) {
        snapshot["components"]["engine.scene.Motion"] =
            motion->to_json(&scene.component<Motion>(entity));
    }
    if (const auto* mesh = registry.find("engine.scene.Mesh");
        mesh != nullptr && scene.has<Mesh>(entity)) {
        snapshot["components"]["engine.scene.Mesh"] = mesh->to_json(&scene.component<Mesh>(entity));
    }
    if (const auto* asset = registry.find("engine.scene.AssetInstance");
        asset != nullptr && scene.has<AssetInstance>(entity)) {
        snapshot["components"]["engine.scene.AssetInstance"] =
            asset->to_json(&scene.component<AssetInstance>(entity));
    }
    if (const auto* light = registry.find("engine.scene.DirectionalLight");
        light != nullptr && scene.has<DirectionalLight>(entity)) {
        snapshot["components"]["engine.scene.DirectionalLight"] =
            light->to_json(&scene.component<DirectionalLight>(entity));
    }
    if (const auto* light = registry.find("engine.scene.PointLight");
        light != nullptr && scene.has<PointLight>(entity)) {
        snapshot["components"]["engine.scene.PointLight"] =
            light->to_json(&scene.component<PointLight>(entity));
    }

    nlohmann::json children = nlohmann::json::array();
    for (const Entity child : scene.children(entity)) {
        children.push_back(entity_snapshot(scene, registry, child));
    }
    snapshot["children"] = std::move(children);
    return snapshot;
}

[[nodiscard]] Result<Entity> restore_snapshot(Scene& scene, const ReflectionRegistry& registry,
                                              const nlohmann::json& snapshot) {
    auto id = Uuid::parse(snapshot.at("id").get<std::string>());
    if (!id) {
        return core::make_failure<Entity>(id.error());
    }
    auto created = scene.create_entity(snapshot.at("name").get<std::string>(), *id);
    if (!created) {
        return created;
    }
    const Entity entity = *created;

    const auto& components = snapshot.at("components");
    const auto* transform = registry.find("engine.scene.Transform");
    const auto* camera = registry.find("engine.scene.Camera");
    if (transform == nullptr || camera == nullptr) {
        return core::make_failure<Entity>(core::make_error(
            ErrorCode::NotFound, "scene component descriptors are not registered"));
    }
    if (const auto iterator = components.find("engine.scene.Transform");
        iterator != components.end()) {
        auto parsed = transform->from_json(&scene.transform(entity), *iterator);
        if (!parsed) {
            return core::make_failure<Entity>(parsed.error());
        }
    }
    if (const auto iterator = components.find("engine.scene.Camera");
        iterator != components.end()) {
        auto added = scene.add_component<Camera>(entity);
        if (!added) {
            return core::make_failure<Entity>(added.error());
        }
        auto parsed = camera->from_json(&scene.component<Camera>(entity), *iterator);
        if (!parsed) {
            return core::make_failure<Entity>(parsed.error());
        }
    }
    if (const auto iterator = components.find("engine.scene.Motion");
        iterator != components.end()) {
        const auto* motion = registry.find("engine.scene.Motion");
        if (motion == nullptr) {
            return core::make_failure<Entity>(
                core::make_error(ErrorCode::NotFound, "motion descriptor missing"));
        }
        auto added = scene.add_component<Motion>(entity);
        if (!added) {
            return core::make_failure<Entity>(added.error());
        }
        auto parsed = motion->from_json(&scene.component<Motion>(entity), *iterator);
        if (!parsed) {
            return core::make_failure<Entity>(parsed.error());
        }
    }
    if (const auto iterator = components.find("engine.scene.Mesh"); iterator != components.end()) {
        const auto* mesh = registry.find("engine.scene.Mesh");
        if (mesh == nullptr) {
            return core::make_failure<Entity>(
                core::make_error(ErrorCode::NotFound, "mesh descriptor missing"));
        }
        auto added = scene.add_component<Mesh>(entity);
        if (!added) {
            return core::make_failure<Entity>(added.error());
        }
        auto parsed = mesh->from_json(&scene.component<Mesh>(entity), *iterator);
        if (!parsed) {
            return core::make_failure<Entity>(parsed.error());
        }
    }
    if (const auto iterator = components.find("engine.scene.AssetInstance");
        iterator != components.end()) {
        const auto* asset = registry.find("engine.scene.AssetInstance");
        if (asset == nullptr) {
            return core::make_failure<Entity>(
                core::make_error(ErrorCode::NotFound, "asset instance descriptor missing"));
        }
        auto added = scene.add_component<AssetInstance>(entity);
        if (!added) {
            return core::make_failure<Entity>(added.error());
        }
        auto parsed = asset->from_json(&scene.component<AssetInstance>(entity), *iterator);
        if (!parsed) {
            return core::make_failure<Entity>(parsed.error());
        }
    }
    if (const auto iterator = snapshot.find("metadata"); iterator != snapshot.end()) {
        auto metadata = scene.set_metadata(entity, *iterator);
        if (!metadata) {
            return core::make_failure<Entity>(metadata.error());
        }
    }
    if (const auto iterator = snapshot.find("parentId");
        iterator != snapshot.end() && !iterator->is_null()) {
        auto parent_id = Uuid::parse(iterator->get<std::string>());
        if (!parent_id) {
            return core::make_failure<Entity>(parent_id.error());
        }
        const auto parent = scene.find(*parent_id);
        if (!parent.has_value()) {
            return core::make_failure<Entity>(
                core::make_error(ErrorCode::NotFound, "parent entity does not exist"));
        }
        auto parent_set = scene.set_parent(entity, *parent);
        if (!parent_set) {
            return core::make_failure<Entity>(parent_set.error());
        }
    }

    if (const auto iterator = snapshot.find("children");
        iterator != snapshot.end() && iterator->is_array()) {
        for (const auto& child : *iterator) {
            auto restored = restore_snapshot(scene, registry, child);
            if (!restored) {
                return core::make_failure<Entity>(restored.error());
            }
        }
    }
    return entity;
}

[[nodiscard]] nlohmann::json scene_command_schemas() {
    return nlohmann::json::parse(R"JSON({
  "scene.entity.create@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["name"],
    "additionalProperties": false,
    "properties": {
      "name": {"type": "string"},
      "id": {"type": "string"},
      "parentId": {"type": ["string", "null"]}
    }
  },
  "scene.entity.delete@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id"],
    "additionalProperties": false,
    "properties": {"id": {"type": "string"}}
  },
  "scene.entity.reparent@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "parentId"],
    "additionalProperties": false,
    "properties": {
      "id": {"type": "string"},
      "parentId": {"type": ["string", "null"]}
    }
  },
  "scene.component.set@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "component", "value"],
    "additionalProperties": false,
    "properties": {
      "id": {"type": "string"},
      "component": {"type": "string"},
      "value": {"type": "object"}
    }
  },
  "scene.component.add@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "component"],
    "additionalProperties": false,
    "properties": {
      "id": {"type": "string"},
      "component": {"type": "string"}
    }
  },
  "scene.component.remove@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "component"],
    "additionalProperties": false,
    "properties": {
      "id": {"type": "string"},
      "component": {"type": "string"}
    }
  },
  "scene.inspect@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": false
  },
  "scene.validate@1": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": false
  }
})JSON");
}

void register_scene_commands(CommandBus& bus, Project& project, Scene& scene,
                             const ReflectionRegistry& registry) {
    const auto assets = asset::register_asset_scene_commands(bus, project.root(), scene);
    if (!assets) {
        throw std::runtime_error("failed to register asset placement: " + assets.error().message);
    }
    const auto add = [&bus](std::string name, nlohmann::json schema,
                            command::CommandRegistry::Handler handler) {
        const auto result = bus.add(std::move(name), std::move(schema), std::move(handler));
        if (!result) {
            throw std::runtime_error("failed to register command: " + result.error().message);
        }
    };

    const nlohmann::json schemas = scene_command_schemas();

    add("scene.entity.create@1", schemas.at("scene.entity.create@1"),
        [&scene](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto parent = parse_parent_argument(arguments);
            if (!parent) {
                return core::make_failure<CommandOutput>(parent.error());
            }
            std::optional<Uuid> requested_id;
            if (arguments.contains("id")) {
                auto parsed = parse_uuid_argument(arguments, "id");
                if (!parsed) {
                    return core::make_failure<CommandOutput>(parsed.error());
                }
                requested_id = *parsed;
            }
            auto created =
                scene.create_entity(arguments.at("name").get<std::string>(), requested_id);
            if (!created) {
                return core::make_failure<CommandOutput>(created.error());
            }
            const Entity entity = *created;
            const auto id = scene.uuid(entity);
            if (!id.has_value()) {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::Internal, "created entity has no UUID"));
            }

            if (parent->has_value()) {
                const auto parent_entity = scene.find(**parent);
                if (!parent_entity.has_value()) {
                    (void)scene.destroy_entity(entity);
                    return core::make_failure<CommandOutput>(
                        core::make_error(ErrorCode::NotFound, "parent entity does not exist"));
                }
                auto parent_set = scene.set_parent(entity, *parent_entity);
                if (!parent_set) {
                    (void)scene.destroy_entity(entity);
                    return core::make_failure<CommandOutput>(parent_set.error());
                }
            }

            const Uuid id_to_redo = *id;
            const std::string name_to_redo = std::string(scene.name(entity));
            const std::optional<Uuid> parent_to_redo = *parent;
            auto undo = [&scene, id_to_redo]() -> Result<void> {
                const auto current = scene.find(id_to_redo);
                if (!current.has_value()) {
                    return {};
                }
                return scene.destroy_entity(*current);
            };
            auto redo = [&scene, id_to_redo, name_to_redo, parent_to_redo]() -> Result<void> {
                auto recreated = scene.create_entity(name_to_redo, id_to_redo);
                if (!recreated) {
                    return core::make_failure<void>(recreated.error());
                }
                if (parent_to_redo.has_value()) {
                    const auto parent_entity = scene.find(*parent_to_redo);
                    if (!parent_entity.has_value()) {
                        return core::make_failure<void>(
                            core::make_error(ErrorCode::NotFound, "parent entity does not exist"));
                    }
                    auto parent_set = scene.set_parent(*recreated, *parent_entity);
                    if (!parent_set) {
                        return parent_set;
                    }
                }
                return {};
            };
            return CommandOutput{
                {{"id", id->to_string()}},
                {},
                std::move(undo),
                std::move(redo),
            };
        });

    add("scene.entity.delete@1", schemas.at("scene.entity.delete@1"),
        [&scene, &registry](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto entity = find_entity(scene, arguments);
            if (!entity) {
                return core::make_failure<CommandOutput>(entity.error());
            }
            const nlohmann::json snapshot = entity_snapshot(scene, registry, *entity);
            auto destroyed = scene.destroy_entity(*entity);
            if (!destroyed) {
                return core::make_failure<CommandOutput>(destroyed.error());
            }
            auto undo = [&scene, &registry, snapshot]() -> Result<void> {
                auto restored = restore_snapshot(scene, registry, snapshot);
                if (!restored) {
                    return core::make_failure<void>(restored.error());
                }
                return {};
            };
            auto redo = [&scene, snapshot]() -> Result<void> {
                auto id = Uuid::parse(snapshot.at("id").get<std::string>());
                if (!id) {
                    return core::make_failure<void>(id.error());
                }
                const auto current = scene.find(*id);
                if (!current.has_value()) {
                    return {};
                }
                return scene.destroy_entity(*current);
            };
            return CommandOutput{nlohmann::json::object(), {}, std::move(undo), std::move(redo)};
        });

    add("scene.entity.reparent@1", schemas.at("scene.entity.reparent@1"),
        [&scene](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto entity = find_entity(scene, arguments);
            auto parent = parse_parent_argument(arguments);
            if (!entity || !parent) {
                return core::make_failure<CommandOutput>(!entity ? entity.error() : parent.error());
            }
            const Entity old_parent = scene.parent(*entity);
            std::optional<Uuid> old_parent_id;
            if (old_parent != entt::null) {
                old_parent_id = scene.uuid(old_parent);
            }
            const auto new_parent = find_parent(scene, *parent);
            if (!new_parent) {
                return core::make_failure<CommandOutput>(new_parent.error());
            }
            auto reparented = scene.set_parent(*entity, *new_parent);
            if (!reparented) {
                return core::make_failure<CommandOutput>(reparented.error());
            }
            const Uuid entity_id = *scene.uuid(*entity);
            auto undo = [&scene, entity_id, old_parent_id]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                if (!old_parent_id.has_value()) {
                    return scene.set_parent(*current, entt::null);
                }
                const auto old_parent_entity = scene.find(*old_parent_id);
                if (!old_parent_entity.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "old parent does not exist"));
                }
                return scene.set_parent(*current, *old_parent_entity);
            };
            auto redo = [&scene, entity_id, parent]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                if (!parent->has_value()) {
                    return scene.set_parent(*current, entt::null);
                }
                const auto parent_entity = scene.find(**parent);
                if (!parent_entity.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "new parent does not exist"));
                }
                return scene.set_parent(*current, *parent_entity);
            };
            return CommandOutput{nlohmann::json::object(), {}, std::move(undo), std::move(redo)};
        });

    add("scene.component.set@1", schemas.at("scene.component.set@1"),
        [&scene, &registry](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto entity = find_entity(scene, arguments);
            if (!entity) {
                return core::make_failure<CommandOutput>(entity.error());
            }
            const std::string component_name = arguments.at("component").get<std::string>();
            const auto* descriptor = registry.find(component_name);
            if (descriptor == nullptr) {
                return core::make_failure<CommandOutput>(core::make_error(
                    ErrorCode::NotFound, "unknown component", {}, {{"component", component_name}}));
            }

            bool was_present = true;
            auto component = mutable_component(scene, *entity, component_name, was_present);
            if (!component) {
                return core::make_failure<CommandOutput>(component.error());
            }

            const nlohmann::json old_value = descriptor->to_json(*component);
            auto parsed = descriptor->from_json(*component, arguments.at("value"));
            if (!parsed) {
                if (!was_present) {
                    (void)remove_component_by_name(scene, *entity, component_name);
                }
                return core::make_failure<CommandOutput>(parsed.error());
            }
            scene.bump_revision();

            const Uuid entity_id = *scene.uuid(*entity);
            const nlohmann::json& new_value = arguments.at("value");
            auto undo = [&scene, &registry, entity_id, component_name, old_value,
                         was_present]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                if (!was_present) {
                    return remove_component_by_name(scene, *current, component_name);
                }
                const auto* desc = registry.find(component_name);
                if (desc == nullptr) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "component descriptor missing"));
                }
                bool present = true;
                auto target = mutable_component(scene, *current, component_name, present);
                if (!target) {
                    return core::make_failure<void>(target.error());
                }
                auto restored = desc->from_json(*target, old_value);
                if (!restored) {
                    return restored;
                }
                scene.bump_revision();
                return {};
            };
            auto redo = [&scene, &registry, entity_id, component_name, new_value,
                         was_present]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                const auto* desc = registry.find(component_name);
                if (desc == nullptr) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "component descriptor missing"));
                }
                bool present = was_present;
                auto target = mutable_component(scene, *current, component_name, present);
                if (!target) {
                    return core::make_failure<void>(target.error());
                }
                auto reapplied = desc->from_json(*target, new_value);
                if (!reapplied) {
                    return reapplied;
                }
                scene.bump_revision();
                return {};
            };
            return CommandOutput{nlohmann::json::object(), {}, std::move(undo), std::move(redo)};
        });

    add("scene.component.add@1", schemas.at("scene.component.add@1"),
        [&scene](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto entity = find_entity(scene, arguments);
            if (!entity) {
                return core::make_failure<CommandOutput>(entity.error());
            }
            const std::string component_name = arguments.at("component").get<std::string>();
            if (component_name == "engine.scene.Transform") {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::Validation, "Transform is always present", {},
                                     {{"component", component_name}}));
            }
            bool was_present = true;
            auto component = mutable_component(scene, *entity, component_name, was_present);
            if (!component) {
                return core::make_failure<CommandOutput>(component.error());
            }
            if (was_present) {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::Conflict, "component already exists", {},
                                     {{"component", component_name}}));
            }
            const Uuid entity_id = *scene.uuid(*entity);
            auto undo = [&scene, entity_id, component_name]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                return remove_component_by_name(scene, *current, component_name);
            };
            auto redo = [&scene, entity_id, component_name]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                bool present = false;
                auto target = mutable_component(scene, *current, component_name, present);
                if (!target) {
                    return core::make_failure<void>(target.error());
                }
                return {};
            };
            return CommandOutput{nlohmann::json::object(), {}, std::move(undo), std::move(redo)};
        });

    add("scene.component.remove@1", schemas.at("scene.component.remove@1"),
        [&scene, &registry](const nlohmann::json& arguments) -> Result<CommandOutput> {
            auto entity = find_entity(scene, arguments);
            if (!entity) {
                return core::make_failure<CommandOutput>(entity.error());
            }
            const std::string component_name = arguments.at("component").get<std::string>();
            if (component_name == "engine.scene.Transform") {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::Validation, "Transform cannot be removed", {},
                                     {{"component", component_name}}));
            }
            if (!has_component_by_name(scene, *entity, component_name)) {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::NotFound, "component does not exist", {},
                                     {{"component", component_name}}));
            }
            const auto* descriptor = registry.find(component_name);
            if (descriptor == nullptr) {
                return core::make_failure<CommandOutput>(
                    core::make_error(ErrorCode::NotFound, "component descriptor missing"));
            }
            bool present = true;
            auto component = mutable_component(scene, *entity, component_name, present);
            if (!component) {
                return core::make_failure<CommandOutput>(component.error());
            }
            const nlohmann::json old_value = descriptor->to_json(*component);
            auto removed = remove_component_by_name(scene, *entity, component_name);
            if (!removed) {
                return core::make_failure<CommandOutput>(removed.error());
            }
            const Uuid entity_id = *scene.uuid(*entity);
            auto undo = [&scene, &registry, entity_id, component_name,
                         old_value]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                const auto* desc = registry.find(component_name);
                if (desc == nullptr) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "component descriptor missing"));
                }
                bool already_present = false;
                auto target = mutable_component(scene, *current, component_name, already_present);
                if (!target) {
                    return core::make_failure<void>(target.error());
                }
                return desc->from_json(*target, old_value);
            };
            auto redo = [&scene, entity_id, component_name]() -> Result<void> {
                const auto current = scene.find(entity_id);
                if (!current.has_value()) {
                    return core::make_failure<void>(
                        core::make_error(ErrorCode::NotFound, "entity does not exist"));
                }
                return remove_component_by_name(scene, *current, component_name);
            };
            return CommandOutput{nlohmann::json::object(), {}, std::move(undo), std::move(redo)};
        });

    add("scene.inspect@1", schemas.at("scene.inspect@1"),
        [&project, &scene, &registry](const nlohmann::json&) -> Result<CommandOutput> {
            auto document = project.scene_json(scene, registry);
            if (!document) {
                return core::make_failure<CommandOutput>(document.error());
            }
            return CommandOutput{std::move(*document)};
        });

    add("scene.validate@1", schemas.at("scene.validate@1"),
        [&scene](const nlohmann::json&) -> Result<CommandOutput> {
            return CommandOutput{{{"valid", true}, {"entities", scene.size()}}};
        });
}

[[nodiscard]] CommandResult execute_scene_command(const std::string& project_root,
                                                  const std::string& scene_path,
                                                  const CommandRequest& request, int undo_count = 0,
                                                  bool dry_run = false) {
    auto project = Project::load(project_root);
    if (!project) {
        return failure(project.error());
    }
    auto locked = project->lock();
    if (!locked) {
        return failure(locked.error());
    }

    ReflectionRegistry registry = make_registry();
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        return failure(scene.error());
    }

    CommandLog log;
    CommandBus bus(log);
    try {
        register_scene_commands(bus, *project, *scene, registry);
    } catch (const std::exception& error) {
        return failure(core::make_error(ErrorCode::Internal, error.what()));
    }

    CommandResult result = bus.execute(request);
    if (!result.ok) {
        return result;
    }
    for (int index = 0; index < undo_count; ++index) {
        auto undone = bus.undo();
        if (!undone) {
            return failure(undone.error());
        }
    }
    if (!dry_run) {
        auto saved = project->save_scene(scene_path, *scene, registry);
        if (!saved) {
            return failure(saved.error());
        }
    }
    return success({{"result", result.data}, {"undoCount", undo_count}, {"dryRun", dry_run}});
}

} // namespace

int project_create(const std::string& root, const std::string& name, bool json, bool dry_run) {
    if (dry_run) {
        std::error_code error;
        const std::filesystem::path target = std::filesystem::weakly_canonical(root, error);
        if (error) {
            const CommandResult result =
                failure(core::make_error(ErrorCode::Io, "could not resolve project root",
                                         error.message(), {{"path", root}}));
            print_result(result, json);
            return exit_code_for(result);
        }
        if (std::filesystem::exists(target / "project.json")) {
            const CommandResult result =
                failure(core::make_error(ErrorCode::Conflict, "project.json already exists", {},
                                         {{"path", target.string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }
        if (std::filesystem::exists(target) && !std::filesystem::is_directory(target)) {
            const CommandResult result =
                failure(core::make_error(ErrorCode::Validation, "project path is not a directory",
                                         {}, {{"path", target.string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }

        std::filesystem::path parent = target;
        while (!std::filesystem::exists(parent) && parent.has_parent_path()) {
            parent = parent.parent_path();
        }
        if (!std::filesystem::is_directory(parent)) {
            const CommandResult result = failure(
                core::make_error(ErrorCode::Io, "project parent is not a writable directory", {},
                                 {{"path", parent.string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }
        const std::filesystem::path probe =
            parent / (".engine-dry-run-" + Uuid::generate().to_string());
        if (!std::filesystem::create_directory(probe, error) || error) {
            const CommandResult result =
                failure(core::make_error(ErrorCode::Io, "project destination is not writable",
                                         error.message(), {{"path", parent.string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }
        std::filesystem::remove(probe, error);
        if (error) {
            const CommandResult result =
                failure(core::make_error(ErrorCode::Io, "could not clean up dry-run probe",
                                         error.message(), {{"path", probe.string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }

        const CommandResult result = success({
            {"dryRun", true},
            {"root", target.string()},
            {"name", name},
            {"scene", "scenes/main.scene.json"},
        });
        print_result(result, json);
        return 0;
    }

    auto project = Project::create(root, name);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result = success({
        {"root", project->root().string()},
        {"projectId", project->project_id().to_string()},
        {"name", project->name()},
        {"scene", "scenes/main.scene.json"},
    });
    print_result(result, json);
    return 0;
}

int project_validate(const std::string& root, bool json) {
    auto project = Project::load(root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry = make_registry();
    std::vector<std::string> warnings;
    for (const auto& scene_path : project->scenes()) {
        auto scene = project->load_scene(scene_path, registry);
        if (!scene) {
            const CommandResult result = failure(scene.error());
            print_result(result, json);
            return exit_code_for(result);
        }
        const auto scene_warnings = point_light_warnings(*scene, scene_path);
        warnings.insert(warnings.end(), scene_warnings.begin(), scene_warnings.end());
    }
    const CommandResult result = success(
        {
            {"valid", true},
            {"projectId", project->project_id().to_string()},
            {"scenes", project->scenes()},
        },
        std::move(warnings));
    print_result(result, json);
    return 0;
}

int project_unlock(const std::string& root, bool force, bool json, bool dry_run) {
    if (!force) {
        const CommandResult result =
            failure(core::make_error(ErrorCode::Validation, "project unlock requires --force"));
        print_result(result, json);
        return exit_code_for(result);
    }
    if (dry_run) {
        const bool lock_exists =
            std::filesystem::exists(std::filesystem::path(root) / ".engine" / "lock");
        const CommandResult result =
            success({{"dryRun", true}, {"root", root}, {"lockExists", lock_exists}});
        print_result(result, json);
        return 0;
    }
    auto unlocked = Project::force_unlock(root);
    if (!unlocked) {
        const CommandResult result = failure(unlocked.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result = success({{"unlocked", true}});
    print_result(result, json);
    return 0;
}

int scene_create(const std::string& project_root, const std::string& name,
                 const std::string& relative_path, bool json, bool dry_run) {
    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto locked = project->lock();
    if (!locked) {
        const CommandResult result = failure(locked.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    auto sandbox = core::PathSandbox::create(project->root());
    if (!sandbox) {
        const CommandResult result = failure(sandbox.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto target = sandbox->resolve(relative_path);
    if (!target) {
        const CommandResult result = failure(target.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    if (std::filesystem::exists(*target) ||
        std::ranges::find(project->scenes(), relative_path) != project->scenes().end()) {
        const CommandResult result = failure(core::make_error(
            ErrorCode::Conflict, "scene already exists", {}, {{"scene", relative_path}}));
        print_result(result, json);
        return exit_code_for(result);
    }
    if (dry_run) {
        const CommandResult result =
            success({{"dryRun", true}, {"scene", relative_path}, {"name", name}});
        print_result(result, json);
        return 0;
    }

    ReflectionRegistry registry = make_registry();
    Scene scene;
    scene.set_scene_name(name);
    auto saved = project->save_scene(relative_path, scene, registry);
    if (!saved) {
        const CommandResult result = failure(saved.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto added = project->add_scene(relative_path);
    if (!added) {
        std::error_code ignored;
        std::filesystem::remove(*target, ignored);
        const CommandResult result = failure(added.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result =
        success({{"scene", relative_path}, {"sceneId", scene.scene_id().to_string()}});
    print_result(result, json);
    return 0;
}

int scene_inspect(const std::string& project_root, const std::string& scene_path, bool json) {
    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry = make_registry();
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        const CommandResult result = failure(scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto document = project->scene_json(*scene, registry);
    if (!document) {
        const CommandResult result = failure(document.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result = success(std::move(*document));
    print_result(result, json);
    return 0;
}

int scene_validate(const std::string& project_root, const std::string& scene_path, bool json) {
    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry = make_registry();
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        const CommandResult result = failure(scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result = success({{"valid", true}, {"entities", scene->size()}},
                                         point_light_warnings(*scene, scene_path));
    print_result(result, json);
    return 0;
}

int entity_create(const std::string& project_root, const std::string& scene_path,
                  const std::string& name, const std::string& parent, const std::string& id,
                  bool json, bool dry_run) {
    nlohmann::json arguments{{"name", name}};
    if (!parent.empty()) {
        arguments["parentId"] = parent == "null" ? nlohmann::json(nullptr) : nlohmann::json(parent);
    }
    if (!id.empty()) {
        arguments["id"] = id;
    }
    const CommandResult result = execute_scene_command(
        project_root, scene_path, CommandRequest{"scene.entity.create@1", arguments}, 0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int entity_delete(const std::string& project_root, const std::string& scene_path,
                  const std::string& id, bool json, bool dry_run) {
    const CommandResult result =
        execute_scene_command(project_root, scene_path,
                              CommandRequest{"scene.entity.delete@1", {{"id", id}}}, 0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int entity_reparent(const std::string& project_root, const std::string& scene_path,
                    const std::string& id, const std::string& parent, bool json, bool dry_run) {
    nlohmann::json arguments{
        {"id", id},
        {"parentId", parent == "null" ? nlohmann::json(nullptr) : nlohmann::json(parent)},
    };
    const CommandResult result = execute_scene_command(
        project_root, scene_path, CommandRequest{"scene.entity.reparent@1", arguments}, 0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int component_set(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component,
                  const std::string& value, bool json, bool dry_run) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(value);
    } catch (const nlohmann::json::exception& error) {
        const CommandResult result = failure(core::make_error(ErrorCode::Validation, error.what()));
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result =
        execute_scene_command(project_root, scene_path,
                              CommandRequest{
                                  "scene.component.set@1",
                                  {{"id", entity_id}, {"component", component}, {"value", parsed}},
                              },
                              0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int component_add(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component, bool json,
                  bool dry_run) {
    const CommandResult result = execute_scene_command(
        project_root, scene_path,
        CommandRequest{"scene.component.add@1", {{"id", entity_id}, {"component", component}}}, 0,
        dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int component_remove(const std::string& project_root, const std::string& scene_path,
                     const std::string& entity_id, const std::string& component, bool json,
                     bool dry_run) {
    const CommandResult result = execute_scene_command(
        project_root, scene_path,
        CommandRequest{"scene.component.remove@1", {{"id", entity_id}, {"component", component}}},
        0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int simulate(const std::string& project_root, const std::string& scene_path, int ticks, bool json) {
    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry = make_registry();
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        const CommandResult result = failure(scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const CommandResult result =
        success(scene::simulate(*scene, ticks, project->settings().tickRate));
    print_result(result, json);
    return 0;
}

int asset_execute(const std::string& operation, const std::string& project_root,
                  const std::string& source, const std::string& asset_id, const std::string& name,
                  bool json, bool dry_run) {
    auto project = Project::load(project_root);
    if (!project) {
        auto result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    if (!dry_run && (operation == "import" || operation == "reimport")) {
        auto sandbox = core::PathSandbox::create(project->root());
        if (!sandbox) {
            const auto result = failure(sandbox.error());
            print_result(result, json);
            return exit_code_for(result);
        }
        auto lock_path = sandbox->resolve(".engine/lock");
        if (!lock_path) {
            const auto result = failure(lock_path.error());
            print_result(result, json);
            return exit_code_for(result);
        }
        auto locked = project->lock();
        if (!locked) {
            auto result = failure(locked.error());
            print_result(result, json);
            return exit_code_for(result);
        }
    }
    CommandLog log;
    CommandBus bus(log);
    auto registered = asset::register_asset_commands(bus, project->root(), dry_run);
    if (!registered) {
        auto result = failure(registered.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    nlohmann::json arguments = nlohmann::json::object();
    if (operation == "import") {
        arguments["source"] = source;
        if (!name.empty()) {
            arguments["name"] = name;
        }
    } else if (operation != "list") {
        arguments["assetId"] = asset_id;
    }
    const auto result = bus.execute({"asset." + operation + "@1", std::move(arguments)});
    print_result(result, json);
    return exit_code_for(result);
}

int asset_place(const std::string& project_root, const std::string& scene_path,
                const std::string& asset_id, const std::string& name,
                const std::vector<double>& position, bool json, bool dry_run) {
    nlohmann::json args = {{"assetId", asset_id}};
    if (!name.empty()) {
        args["name"] = name;
    }
    if (!position.empty()) {
        args["position"] = position;
    }
    const auto result = execute_scene_command(project_root, scene_path,
                                              {"asset.place@1", std::move(args)}, 0, dry_run);
    print_result(result, json);
    return exit_code_for(result);
}

int schema(bool json) {
    const ReflectionRegistry registry = make_registry();
    auto commands = scene_command_schemas();
    commands.update(asset::asset_command_schemas());
    const CommandResult result =
        success({{"components", registry.schemas()}, {"commands", std::move(commands)}});
    print_result(result, json);
    return 0;
}

int batch(const std::string& project_root, const std::string& scene_path, const std::string& file,
          int undo_count, bool json, bool dry_run) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        const CommandResult result = failure(
            core::make_error(ErrorCode::Io, "could not open batch file", {}, {{"path", file}}));
        print_result(result, json);
        return exit_code_for(result);
    }

    nlohmann::json document;
    try {
        input >> document;
    } catch (const nlohmann::json::exception& error) {
        const CommandResult result = failure(core::make_error(ErrorCode::Validation, error.what()));
        print_result(result, json);
        return exit_code_for(result);
    }

    const auto commands_iterator = document.find("commands");
    if (commands_iterator == document.end() || !commands_iterator->is_array()) {
        const CommandResult result = failure(
            core::make_error(ErrorCode::Validation, "batch file must contain a commands array"));
        print_result(result, json);
        return exit_code_for(result);
    }

    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto locked = project->lock();
    if (!locked) {
        const CommandResult result = failure(locked.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    ReflectionRegistry registry = make_registry();
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        const CommandResult result = failure(scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    CommandLog log;
    CommandBus bus(log);
    try {
        register_scene_commands(bus, *project, *scene, registry);
    } catch (const std::exception& error) {
        const CommandResult result = failure(core::make_error(ErrorCode::Internal, error.what()));
        print_result(result, json);
        return exit_code_for(result);
    }

    auto transaction = bus.begin_transaction();
    if (!transaction) {
        const CommandResult result = failure(transaction.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    nlohmann::json results = nlohmann::json::array();
    for (const auto& command : *commands_iterator) {
        if (!command.is_object() || !command.contains("name") || !command["name"].is_string()) {
            (void)bus.rollback_transaction();
            const CommandResult result = failure(core::make_error(
                ErrorCode::Validation, "each batch command needs a name and arguments"));
            print_result(result, json);
            return exit_code_for(result);
        }
        CommandRequest request;
        request.name = command["name"].get<std::string>();
        if (const auto arguments = command.find("arguments"); arguments != command.end()) {
            request.arguments = *arguments;
        }
        const CommandResult result = bus.execute(request);
        results.push_back(result.to_json());
        if (!result.ok) {
            (void)bus.rollback_transaction();
            CommandResult failure_result = failure(result.error.value_or(
                core::make_error(ErrorCode::Internal, "batch command failed")));
            failure_result.data = {{"results", std::move(results)}, {"rolledBack", true}};
            print_result(failure_result, json);
            return exit_code_for(failure_result);
        }
    }

    auto committed = bus.commit_transaction();
    if (!committed) {
        const CommandResult result = failure(committed.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    for (int index = 0; index < undo_count; ++index) {
        auto undone = bus.undo();
        if (!undone) {
            const CommandResult result = failure(undone.error());
            print_result(result, json);
            return exit_code_for(result);
        }
    }

    if (!dry_run) {
        auto saved = project->save_scene(scene_path, *scene, registry);
        if (!saved) {
            const CommandResult result = failure(saved.error());
            print_result(result, json);
            return exit_code_for(result);
        }
    }
    const CommandResult result =
        success({{"results", std::move(results)}, {"undoCount", undo_count}, {"dryRun", dry_run}});
    print_result(result, json);
    return 0;
}

int screenshot(const std::string& project_root, const std::string& scene_path,
               const std::string& out_path, int frames, int width, int height, bool json) {
    if (out_path.empty()) {
        const CommandResult result = failure(
            core::make_error(ErrorCode::Usage, "--out is required", {}, {{"stage", "screenshot"}}));
        print_result(result, json);
        return exit_code_for(result);
    }
    constexpr int kMaxDimension = 8192;
    constexpr int kMaxFrames = 1000;
    if (frames <= 0 || width <= 0 || height <= 0) {
        const CommandResult result = failure(
            core::make_error(ErrorCode::Validation, "frames, width and height must be positive", {},
                             {{"frames", frames}, {"width", width}, {"height", height}}));
        print_result(result, json);
        return exit_code_for(result);
    }
    if (frames > kMaxFrames || width > kMaxDimension || height > kMaxDimension) {
        const CommandResult result =
            failure(core::make_error(ErrorCode::Validation, "screenshot limits exceeded", {},
                                     {{"maxFrames", kMaxFrames}, {"maxDimension", kMaxDimension}}));
        print_result(result, json);
        return exit_code_for(result);
    }

    auto project = Project::load(project_root);
    if (!project) {
        const CommandResult result = failure(project.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto sandbox = core::PathSandbox::create(project->root());
    if (!sandbox) {
        const CommandResult result = failure(sandbox.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    auto output = sandbox->resolve(out_path);
    if (!output) {
        const CommandResult result = failure(output.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    ReflectionRegistry registry;
    scene::register_scene_components(registry);
    auto loaded_scene = project->load_scene(scene_path, registry);
    if (!loaded_scene) {
        const CommandResult result = failure(loaded_scene.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    render::DeviceOptions device_options;
    device_options.application_name = "engine screenshot";
    auto device = render::Device::create(device_options);
    if (!device) {
        const CommandResult result = failure(device.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    render::OffscreenOptions offscreen_options;
    offscreen_options.width = static_cast<std::uint32_t>(width);
    offscreen_options.height = static_cast<std::uint32_t>(height);
    auto renderer = render::OffscreenRenderer::create(*device, offscreen_options);
    if (!renderer) {
        const CommandResult result = failure(renderer.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    const scene::SceneView view(*loaded_scene);
    const auto uploaded = upload_scene_assets(*renderer, project->root(), view);
    if (!uploaded) {
        const auto result = failure(uploaded.error());
        print_result(result, json);
        return exit_code_for(result);
    }
    const auto warnings = point_light_warnings(*loaded_scene, scene_path);
    auto pixels = renderer->render(view, frames);
    if (!pixels) {
        const CommandResult result = failure(pixels.error());
        print_result(result, json);
        return exit_code_for(result);
    }

    if (output->has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output->parent_path(), error);
        if (error) {
            const CommandResult result = failure(
                core::make_error(ErrorCode::Io, "could not create the screenshot directory",
                                 error.message(), {{"path", output->parent_path().string()}}));
            print_result(result, json);
            return exit_code_for(result);
        }
    }
    if (stbi_write_png(output->string().c_str(), width, height, 4, pixels->data(), width * 4) ==
        0) {
        const CommandResult result = failure(core::make_error(
            ErrorCode::Io, "could not write the screenshot", {}, {{"path", output->string()}}));
        print_result(result, json);
        return exit_code_for(result);
    }

    const CommandResult result = success(
        {
            {"path", output->string()},
            {"width", width},
            {"height", height},
            {"frames", frames},
        },
        warnings);
    print_result(result, json);
    return exit_code_for(result);
}

} // namespace engine::cli
