#pragma once

#include <glm/glm.hpp>

#include <array>

namespace engine::render {

// Six normalized frustum planes extracted from a view-projection matrix.
class Frustum {
public:
    [[nodiscard]] static Frustum from_view_projection(const glm::mat4& view_projection);

    [[nodiscard]] bool intersects_sphere(const glm::vec3& center, float radius) const noexcept;

private:
    std::array<glm::vec4, 6> planes_{};
};

} // namespace engine::render
