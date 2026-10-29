#pragma once

#include <engine/scene/scene.hpp>

#include <nlohmann/json.hpp>

namespace engine::scene {

[[nodiscard]] nlohmann::json simulate(Scene& scene, int ticks, int tickRate);

} // namespace engine::scene
