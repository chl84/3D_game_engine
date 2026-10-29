#pragma once

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

} // namespace engine::core
