#include <engine/render/frustum.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

namespace engine::render {
namespace {

[[nodiscard]] glm::vec4 row(const glm::mat4& matrix, int index) noexcept {
    return {matrix[0][index], matrix[1][index], matrix[2][index], matrix[3][index]};
}

void normalize_plane(glm::vec4& plane) noexcept {
    const glm::vec3 normal{plane.x, plane.y, plane.z};
    const float length = glm::length(normal);
    if (length > 0.0F) {
        plane /= length;
    }
}

} // namespace

Frustum Frustum::from_view_projection(const glm::mat4& view_projection) {
    const glm::vec4 row0 = row(view_projection, 0);
    const glm::vec4 row1 = row(view_projection, 1);
    const glm::vec4 row2 = row(view_projection, 2);
    const glm::vec4 row3 = row(view_projection, 3);

    Frustum frustum;
    frustum.planes_ = {
        row3 + row0, // left
        row3 - row0, // right
        row3 + row1, // bottom
        row3 - row1, // top
        row2,        // near (Vulkan depth range [0, 1])
        row3 - row2, // far
    };
    for (glm::vec4& plane : frustum.planes_) {
        normalize_plane(plane);
    }
    return frustum;
}

bool Frustum::intersects_sphere(const glm::vec3& center, float radius) const noexcept {
    return std::ranges::all_of(planes_, [&center, radius](const glm::vec4& plane) {
        const float distance = glm::dot(glm::vec3{plane.x, plane.y, plane.z}, center) + plane.w;
        return distance >= -radius;
    });
}

} // namespace engine::render
