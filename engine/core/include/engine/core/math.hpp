#pragma once

#include <cmath>

namespace engine::core {

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    [[nodiscard]] bool operator==(const Vec3&) const = default;
};

struct Quat {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};

    [[nodiscard]] bool operator==(const Quat&) const = default;
};

[[nodiscard]] inline Vec3 operator+(const Vec3& left, const Vec3& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline Vec3 operator-(const Vec3& left, const Vec3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline Vec3 operator*(const Vec3& value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline Vec3 component_mul(const Vec3& left, const Vec3& right) noexcept {
    return {left.x * right.x, left.y * right.y, left.z * right.z};
}

[[nodiscard]] inline Quat operator*(const Quat& left, const Quat& right) noexcept {
    return {
        (left.w * right.x) + (left.x * right.w) + (left.y * right.z) - (left.z * right.y),
        (left.w * right.y) - (left.x * right.z) + (left.y * right.w) + (left.z * right.x),
        (left.w * right.z) + (left.x * right.y) - (left.y * right.x) + (left.z * right.w),
        (left.w * right.w) - (left.x * right.x) - (left.y * right.y) - (left.z * right.z),
    };
}

[[nodiscard]] inline Vec3 cross(const Vec3& left, const Vec3& right) noexcept {
    return {
        (left.y * right.z) - (left.z * right.y),
        (left.z * right.x) - (left.x * right.z),
        (left.x * right.y) - (left.y * right.x),
    };
}

[[nodiscard]] inline Vec3 rotate(const Quat& rotation, const Vec3& value) noexcept {
    const Vec3 q{rotation.x, rotation.y, rotation.z};
    const Vec3 uv = cross(q, value);
    const Vec3 uuv = cross(q, uv);
    return value + (uv * (2.0F * rotation.w)) + (uuv * 2.0F);
}

struct Vec3d {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] bool operator==(const Vec3d&) const = default;
};

struct Quatd {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{1.0};

    [[nodiscard]] bool operator==(const Quatd&) const = default;
};

[[nodiscard]] inline Vec3d operator+(const Vec3d& left, const Vec3d& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline Vec3d operator-(const Vec3d& left, const Vec3d& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline Vec3d operator*(const Vec3d& value, double scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline Vec3d component_mul(const Vec3d& left, const Vec3d& right) noexcept {
    return {left.x * right.x, left.y * right.y, left.z * right.z};
}

[[nodiscard]] inline Quatd operator*(const Quatd& left, const Quatd& right) noexcept {
    return {
        (left.w * right.x) + (left.x * right.w) + (left.y * right.z) - (left.z * right.y),
        (left.w * right.y) - (left.x * right.z) + (left.y * right.w) + (left.z * right.x),
        (left.w * right.z) + (left.x * right.y) - (left.y * right.x) + (left.z * right.w),
        (left.w * right.w) - (left.x * right.x) - (left.y * right.y) - (left.z * right.z),
    };
}

[[nodiscard]] inline Vec3d cross(const Vec3d& left, const Vec3d& right) noexcept {
    return {
        (left.y * right.z) - (left.z * right.y),
        (left.z * right.x) - (left.x * right.z),
        (left.x * right.y) - (left.y * right.x),
    };
}

[[nodiscard]] inline Vec3d rotate(const Quatd& rotation, const Vec3d& value) noexcept {
    const Vec3d q{rotation.x, rotation.y, rotation.z};
    const Vec3d uv = cross(q, value);
    const Vec3d uuv = cross(q, uv);
    return value + (uv * (2.0 * rotation.w)) + (uuv * 2.0);
}

[[nodiscard]] inline Quatd normalize(const Quatd& rotation) noexcept {
    const double length = std::sqrt((rotation.x * rotation.x) + (rotation.y * rotation.y) +
                                    (rotation.z * rotation.z) + (rotation.w * rotation.w));
    if (length <= 0.0) {
        return Quatd{};
    }
    return {rotation.x / length, rotation.y / length, rotation.z / length, rotation.w / length};
}

} // namespace engine::core
