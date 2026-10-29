#pragma once

#include <glm/glm.hpp>

namespace engine::render {

// A right-handed camera with a Vulkan-style projection: Y points down and the
// depth range is [0, 1].
struct Camera {
    glm::vec3 position{0.0F, 0.0F, 2.0F};
    glm::vec3 target{0.0F, 0.0F, 0.0F};
    glm::vec3 up{0.0F, 1.0F, 0.0F};
    float vertical_fov_radians{1.0471976F};
    float near_plane{0.1F};
    float far_plane{100.0F};

    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 projection(float aspect) const;
    [[nodiscard]] glm::mat4 view_projection(float aspect) const;
};

} // namespace engine::render
