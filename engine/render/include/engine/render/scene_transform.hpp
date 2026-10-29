#pragma once

#include <engine/scene/scene.hpp>

#include <glm/glm.hpp>

namespace engine::render {

// Builds a model matrix relative to the camera. The subtraction happens in
// double precision before the result is converted to float.
[[nodiscard]] glm::mat4 camera_relative_model(const scene::WorldTransform& world,
                                              const core::Vec3d& camera_position);

// Builds a rotation-only view matrix for a camera at the origin. The camera's
// world position is applied by camera_relative_model instead.
[[nodiscard]] glm::mat4 camera_view_from_world(const scene::WorldTransform& camera_world);

} // namespace engine::render
