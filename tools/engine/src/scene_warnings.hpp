#pragma once

#include <engine/scene/scene.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace engine::cli {

// PointLight is stored and validated from scene v3, but the renderer ignores it
// until M5. Every command that loads a scene surfaces this instead of silently
// dropping the light.
[[nodiscard]] inline std::vector<std::string> point_light_warnings(const scene::Scene& scene,
                                                                   std::string_view scene_path) {
    std::size_t count = 0;
    for (const scene::Entity entity : scene.entities()) {
        if (scene.has<scene::PointLight>(entity)) {
            ++count;
        }
    }
    if (count == 0) {
        return {};
    }
    return {
        "scene '" + std::string(scene_path) + "' contains " + std::to_string(count) +
            " PointLight component(s); point lights are not rendered until M5",
    };
}

} // namespace engine::cli
