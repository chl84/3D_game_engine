#include <engine/render/camera.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace engine::render {

glm::mat4 Camera::view() const {
    return glm::lookAtRH(position, target, up);
}

glm::mat4 Camera::projection(float aspect) const {
    const float safe_aspect = std::max(aspect, 0.0001F);
    glm::mat4 matrix =
        glm::perspectiveRH_ZO(vertical_fov_radians, safe_aspect, near_plane, far_plane);
    // Vulkan clip space has Y pointing down.
    matrix[1][1] *= -1.0F;
    return matrix;
}

glm::mat4 Camera::view_projection(float aspect) const {
    return projection(aspect) * view();
}

} // namespace engine::render
