#pragma once

#include <engine/core/math.hpp>
#include <engine/core/reflect.hpp>
#include <engine/core/uuid.hpp>

#include <entt/entt.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::scene {

using core::Error;
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

    [[nodiscard]] Entity parent(Entity entity) const noexcept;
    [[nodiscard]] Result<void> set_parent(Entity child, Entity parent);
    [[nodiscard]] const std::vector<Entity>& children(Entity entity) const;

    [[nodiscard]] Transform& transform(Entity entity);
    [[nodiscard]] const Transform& transform(Entity entity) const;
    [[nodiscard]] Transform world_transform(Entity entity) const;

    [[nodiscard]] entt::registry& registry() noexcept;
    [[nodiscard]] const entt::registry& registry() const noexcept;

private:
    [[nodiscard]] Entity parent_of(Entity entity) const noexcept;

    entt::registry registry_;
    std::unordered_map<core::Uuid, Entity> by_uuid_;
    std::function<core::Uuid()> next_uuid_;
};

void register_scene_components(core::ReflectionRegistry& registry);

} // namespace engine::scene
