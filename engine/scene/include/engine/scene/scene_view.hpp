#pragma once

#include <engine/scene/scene.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace engine::scene {

// Read-only view over a Scene. The scene must outlive the view. The renderer
// receives this type instead of Scene or SceneEditor.
class SceneView {
public:
    explicit SceneView(const Scene& scene) noexcept;

    [[nodiscard]] const Scene& scene() const noexcept;
    [[nodiscard]] std::vector<Entity> entities() const;
    [[nodiscard]] bool contains(Entity entity) const noexcept;
    [[nodiscard]] std::string_view name(Entity entity) const;

    [[nodiscard]] WorldTransform world_transform(Entity entity) const;

    [[nodiscard]] bool has_mesh(Entity entity) const noexcept;
    [[nodiscard]] const Mesh& mesh(Entity entity) const;
    [[nodiscard]] bool has_camera(Entity entity) const noexcept;
    [[nodiscard]] const Camera& camera(Entity entity) const;
    [[nodiscard]] std::optional<Entity> active_camera() const;

private:
    const Scene* scene_;
};

} // namespace engine::scene
