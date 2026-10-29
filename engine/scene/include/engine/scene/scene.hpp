#pragma once

#include <engine/core/math.hpp>
#include <engine/core/reflect.hpp>
#include <engine/core/uuid.hpp>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::scene {

using core::Error;
using core::ErrorCode;
using core::Result;
using Entity = entt::entity;

struct Name {
    std::string value;
};

struct UuidComponent {
    core::Uuid value;
};

struct Parent {
    Entity entity{entt::null};
};

struct Children {
    std::vector<Entity> entities;
};

struct Transform {
    core::Vec3 position;
    core::Quat rotation;
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct Camera {
    float fovYRadians{1.0471975511965976F};
    float near{0.1F};
    float far{1000.0F};
    bool enabled{true};
};

struct EntityMetadata {
    nlohmann::json value = nlohmann::json::object();
};

class Scene {
public:
    Scene();
    explicit Scene(std::uint64_t seed);

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    [[nodiscard]] static Scene with_seed(std::uint64_t seed);

    [[nodiscard]] Result<Entity> create_entity(std::string name,
                                               std::optional<core::Uuid> id = std::nullopt);
    [[nodiscard]] Result<void> destroy_entity(Entity entity);

    [[nodiscard]] bool contains(Entity entity) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] std::optional<core::Uuid> uuid(Entity entity) const;
    [[nodiscard]] std::optional<Entity> find(const core::Uuid& id) const;
    [[nodiscard]] std::vector<Entity> entities() const;

    [[nodiscard]] std::string_view name(Entity entity) const;
    [[nodiscard]] Result<void> set_name(Entity entity, std::string name);
    [[nodiscard]] const nlohmann::json& metadata(Entity entity) const;
    [[nodiscard]] Result<void> set_metadata(Entity entity, nlohmann::json metadata);

    [[nodiscard]] Entity parent(Entity entity) const noexcept;
    [[nodiscard]] Result<void> set_parent(Entity child, Entity parent);
    [[nodiscard]] const std::vector<Entity>& children(Entity entity) const;

    [[nodiscard]] Transform& transform(Entity entity);
    [[nodiscard]] const Transform& transform(Entity entity) const;
    [[nodiscard]] Transform world_transform(Entity entity) const;

    template <typename T> [[nodiscard]] bool has(Entity entity) const noexcept {
        return contains(entity) && registry_.all_of<T>(entity);
    }

    template <typename T> [[nodiscard]] T& component(Entity entity) {
        return registry_.get<T>(entity);
    }

    template <typename T> [[nodiscard]] const T& component(Entity entity) const {
        return registry_.get<T>(entity);
    }

    template <typename T> [[nodiscard]] Result<void> add_component(Entity entity) {
        if (!contains(entity)) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::NotFound, "entity does not exist"));
        }
        if (registry_.all_of<T>(entity)) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::Conflict, "component already exists"));
        }
        registry_.emplace<T>(entity);
        bump_revision();
        return {};
    }

    template <typename T> [[nodiscard]] Result<void> remove_component(Entity entity) {
        if (!has<T>(entity)) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::NotFound, "component does not exist"));
        }
        registry_.remove<T>(entity);
        bump_revision();
        return {};
    }

    [[nodiscard]] const core::Uuid& scene_id() const noexcept;
    void set_scene_id(const core::Uuid& id);
    [[nodiscard]] const std::string& scene_name() const noexcept;
    void set_scene_name(std::string name);
    [[nodiscard]] const nlohmann::json& scene_metadata() const noexcept;
    void set_scene_metadata(nlohmann::json metadata);
    [[nodiscard]] std::uint64_t revision() const noexcept;
    void set_revision(std::uint64_t revision) noexcept;
    void bump_revision() noexcept;

    [[nodiscard]] entt::registry& registry() noexcept;
    [[nodiscard]] const entt::registry& registry() const noexcept;

private:
    [[nodiscard]] Entity parent_of(Entity entity) const noexcept;

    entt::registry registry_;
    std::unordered_map<core::Uuid, Entity> by_uuid_;
    std::function<core::Uuid()> next_uuid_;
    core::Uuid scene_id_;
    std::string scene_name_{"main"};
    nlohmann::json scene_metadata_ = nlohmann::json::object();
    std::uint64_t revision_{0};
};

void register_scene_components(core::ReflectionRegistry& registry);

} // namespace engine::scene
