#include <engine/scene/scene.hpp>

#include <algorithm>
#include <utility>

namespace engine::scene {
namespace {

[[nodiscard]] Error not_found(Entity entity) {
    return core::make_error(core::ErrorCode::NotFound, "entity does not exist", {},
                            {{"entity", static_cast<std::uint32_t>(entity)}});
}

[[nodiscard]] Error duplicate_uuid(const core::Uuid& id) {
    return core::make_error(core::ErrorCode::Conflict, "entity UUID already exists", {},
                            {{"uuid", id.to_string()}});
}

} // namespace

Scene::Scene()
    : next_uuid_([] { return core::Uuid::generate(); }), scene_id_(core::Uuid::generate()) {}

Scene::Scene(std::uint64_t seed)
    : next_uuid_([generator = core::UuidGenerator(seed)]() mutable { return generator.next(); }),
      scene_id_(next_uuid_()) {}

Scene Scene::with_seed(std::uint64_t seed) {
    return Scene(seed);
}

Result<Entity> Scene::create_entity(std::string name, std::optional<core::Uuid> id) {
    core::Uuid value;
    if (id.has_value()) {
        if (by_uuid_.contains(*id)) {
            return core::make_failure<Entity>(duplicate_uuid(*id));
        }
        value = *id;
    } else {
        do {
            value = next_uuid_();
        } while (by_uuid_.contains(value));
    }

    const Entity entity = registry_.create();
    registry_.emplace<Name>(entity, Name{std::move(name)});
    registry_.emplace<UuidComponent>(entity, UuidComponent{value});
    registry_.emplace<Parent>(entity, Parent{entt::null});
    registry_.emplace<Children>(entity, Children{});
    registry_.emplace<Transform>(entity, Transform{});
    registry_.emplace<EntityMetadata>(entity, EntityMetadata{});
    by_uuid_.emplace(value, entity);
    bump_revision();
    return entity;
}

Result<void> Scene::destroy_entity(Entity entity) {
    if (!contains(entity)) {
        return core::make_failure<void>(not_found(entity));
    }

    const std::vector<Entity> child_entities = registry_.get<Children>(entity).entities;
    for (const Entity child : child_entities) {
        if (contains(child)) {
            auto result = destroy_entity(child);
            if (!result) {
                return result;
            }
        }
    }

    const Entity parent_entity = parent_of(entity);
    if (parent_entity != entt::null && contains(parent_entity)) {
        auto& siblings = registry_.get<Children>(parent_entity).entities;
        std::erase(siblings, entity);
    }

    if (const auto* id = registry_.try_get<UuidComponent>(entity); id != nullptr) {
        by_uuid_.erase(id->value);
    }
    registry_.destroy(entity);
    bump_revision();
    return {};
}

bool Scene::contains(Entity entity) const noexcept {
    return registry_.valid(entity) && registry_.all_of<UuidComponent>(entity);
}

std::size_t Scene::size() const noexcept {
    return by_uuid_.size();
}

std::optional<core::Uuid> Scene::uuid(Entity entity) const {
    if (!contains(entity)) {
        return std::nullopt;
    }
    return registry_.get<UuidComponent>(entity).value;
}

std::optional<Entity> Scene::find(const core::Uuid& id) const {
    const auto iterator = by_uuid_.find(id);
    return iterator == by_uuid_.end() ? std::nullopt : std::optional<Entity>(iterator->second);
}

std::vector<Entity> Scene::entities() const {
    std::vector<Entity> result;
    result.reserve(by_uuid_.size());
    for (const auto [entity, component] : registry_.view<UuidComponent>().each()) {
        (void)component;
        result.push_back(entity);
    }
    std::ranges::sort(result, [this](Entity left, Entity right) {
        return registry_.get<UuidComponent>(left).value < registry_.get<UuidComponent>(right).value;
    });
    return result;
}

std::string_view Scene::name(Entity entity) const {
    if (!contains(entity)) {
        return {};
    }
    return registry_.get<Name>(entity).value;
}

Result<void> Scene::set_name(Entity entity, std::string name) {
    if (!contains(entity)) {
        return core::make_failure<void>(not_found(entity));
    }
    registry_.get<Name>(entity).value = std::move(name);
    bump_revision();
    return {};
}

const nlohmann::json& Scene::metadata(Entity entity) const {
    static const nlohmann::json empty = nlohmann::json::object();
    if (!contains(entity)) {
        return empty;
    }
    return registry_.get<EntityMetadata>(entity).value;
}

Result<void> Scene::set_metadata(Entity entity, nlohmann::json metadata) {
    if (!contains(entity)) {
        return core::make_failure<void>(not_found(entity));
    }
    registry_.get<EntityMetadata>(entity).value = std::move(metadata);
    bump_revision();
    return {};
}

Entity Scene::parent(Entity entity) const noexcept {
    return parent_of(entity);
}

Result<void> Scene::set_parent(Entity child, Entity parent) {
    if (!contains(child) || (parent != entt::null && !contains(parent))) {
        return core::make_failure<void>(not_found(parent == entt::null ? child : parent));
    }
    if (child == parent) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Validation, "an entity cannot be its own parent"));
    }

    for (Entity current = parent; current != entt::null; current = parent_of(current)) {
        if (current == child) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::Validation, "parenting would create a cycle"));
        }
    }

    const Entity old_parent = parent_of(child);
    if (old_parent == parent) {
        return {};
    }
    if (old_parent != entt::null) {
        auto& siblings = registry_.get<Children>(old_parent).entities;
        std::erase(siblings, child);
    }

    registry_.get<Parent>(child).entity = parent;
    if (parent != entt::null) {
        registry_.get<Children>(parent).entities.push_back(child);
    }
    bump_revision();
    return {};
}

const std::vector<Entity>& Scene::children(Entity entity) const {
    static const std::vector<Entity> empty;
    if (!contains(entity)) {
        return empty;
    }
    return registry_.get<Children>(entity).entities;
}

Transform& Scene::transform(Entity entity) {
    return registry_.get<Transform>(entity);
}

const Transform& Scene::transform(Entity entity) const {
    return registry_.get<Transform>(entity);
}

WorldTransform Scene::world_transform(Entity entity) const {
    const Transform& local = registry_.get<Transform>(entity);
    WorldTransform local_world{
        {
            static_cast<double>(local.position.x),
            static_cast<double>(local.position.y),
            static_cast<double>(local.position.z),
        },
        {
            static_cast<double>(local.rotation.x),
            static_cast<double>(local.rotation.y),
            static_cast<double>(local.rotation.z),
            static_cast<double>(local.rotation.w),
        },
        {
            static_cast<double>(local.scale.x),
            static_cast<double>(local.scale.y),
            static_cast<double>(local.scale.z),
        },
    };

    const Entity parent_entity = parent_of(entity);
    if (parent_entity == entt::null) {
        local_world.rotation = core::normalize(local_world.rotation);
        return local_world;
    }

    const WorldTransform parent_world = world_transform(parent_entity);
    return {
        parent_world.position +
            core::rotate(parent_world.rotation,
                         core::component_mul(local_world.position, parent_world.scale)),
        core::normalize(parent_world.rotation * local_world.rotation),
        core::component_mul(parent_world.scale, local_world.scale),
    };
}

const core::Uuid& Scene::scene_id() const noexcept {
    return scene_id_;
}

void Scene::set_scene_id(const core::Uuid& id) {
    scene_id_ = id;
}

const std::string& Scene::scene_name() const noexcept {
    return scene_name_;
}

void Scene::set_scene_name(std::string name) {
    scene_name_ = std::move(name);
}

const nlohmann::json& Scene::scene_metadata() const noexcept {
    return scene_metadata_;
}

void Scene::set_scene_metadata(nlohmann::json metadata) {
    scene_metadata_ = std::move(metadata);
}

std::uint64_t Scene::revision() const noexcept {
    return revision_;
}

void Scene::set_revision(std::uint64_t revision) noexcept {
    revision_ = revision;
}

void Scene::bump_revision() noexcept {
    ++revision_;
}

entt::registry& Scene::registry() noexcept {
    return registry_;
}

const entt::registry& Scene::registry() const noexcept {
    return registry_;
}

Entity Scene::parent_of(Entity entity) const noexcept {
    if (!contains(entity)) {
        return entt::null;
    }
    return registry_.get<Parent>(entity).entity;
}

void register_scene_components(core::ReflectionRegistry& registry) {
    registry.component<Transform>("engine.scene.Transform")
        .field("position", &Transform::position, "Local position in metres")
        .field("rotation", &Transform::rotation, "Local rotation quaternion (x, y, z, w)")
        .field("scale", &Transform::scale, "Local scale")
        .build();

    registry.component<Camera>("engine.scene.Camera")
        .field("fovYRadians", &Camera::fovYRadians, "Vertical field of view in radians")
        .field("near", &Camera::near, "Near plane")
        .field("far", &Camera::far, "Far plane")
        .field("enabled", &Camera::enabled, "Whether the camera is active")
        .build();

    registry.component<Motion>("engine.scene.Motion")
        .field("linearVelocity", &Motion::linearVelocity, "Linear velocity in metres per second")
        .build();

    registry.component<Mesh>("engine.scene.Mesh")
        .field("meshId", &Mesh::mesh_id, "Asset ID or built-in mesh ID")
        .build();
}

} // namespace engine::scene
