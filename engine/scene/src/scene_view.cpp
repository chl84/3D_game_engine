#include <engine/scene/scene_view.hpp>

namespace engine::scene {

SceneView::SceneView(const Scene& scene) noexcept : scene_(&scene) {}

const Scene& SceneView::scene() const noexcept {
    return *scene_;
}

std::vector<Entity> SceneView::entities() const {
    return scene_->entities();
}

bool SceneView::contains(Entity entity) const noexcept {
    return scene_->contains(entity);
}

std::string_view SceneView::name(Entity entity) const {
    return scene_->name(entity);
}

WorldTransform SceneView::world_transform(Entity entity) const {
    return scene_->world_transform(entity);
}

bool SceneView::has_mesh(Entity entity) const noexcept {
    return scene_->has<Mesh>(entity);
}

const Mesh& SceneView::mesh(Entity entity) const {
    return scene_->component<Mesh>(entity);
}

bool SceneView::has_camera(Entity entity) const noexcept {
    return scene_->has<Camera>(entity);
}

const Camera& SceneView::camera(Entity entity) const {
    return scene_->component<Camera>(entity);
}

std::optional<Entity> SceneView::active_camera() const {
    for (const Entity entity : scene_->entities()) {
        if (!scene_->has<Camera>(entity)) {
            continue;
        }
        if (scene_->component<Camera>(entity).enabled) {
            return entity;
        }
    }
    return std::nullopt;
}

} // namespace engine::scene
