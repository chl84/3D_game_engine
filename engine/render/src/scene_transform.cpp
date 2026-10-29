#include <engine/render/scene_transform.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::render {
namespace {

[[nodiscard]] glm::vec3 to_vec3(const core::Vec3d& value) noexcept {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

[[nodiscard]] glm::quat to_quat(const core::Quatd& rotation) noexcept {
    const core::Quatd normalized = core::normalize(rotation);
    return {static_cast<float>(normalized.w), static_cast<float>(normalized.x),
            static_cast<float>(normalized.y), static_cast<float>(normalized.z)};
}

} // namespace

glm::mat4 camera_relative_model(const scene::WorldTransform& world,
                                const core::Vec3d& camera_position) {
    const core::Vec3d relative = world.position - camera_position;
    glm::mat4 model = glm::translate(glm::mat4(1.0F), to_vec3(relative));
    model *= glm::mat4_cast(to_quat(world.rotation));
    return glm::scale(model, to_vec3(world.scale));
}

glm::mat4 camera_view_from_world(const scene::WorldTransform& camera_world) {
    const core::Quatd rotation = core::normalize(camera_world.rotation);
    const core::Vec3d forward = core::rotate(rotation, core::Vec3d{0.0, 0.0, -1.0});
    const core::Vec3d up = core::rotate(rotation, core::Vec3d{0.0, 1.0, 0.0});
    return glm::lookAtRH(glm::vec3{0.0F, 0.0F, 0.0F}, to_vec3(forward), to_vec3(up));
}

} // namespace engine::render
