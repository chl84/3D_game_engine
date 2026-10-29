#include <engine/scene/simulation.hpp>

#include <utility>

namespace engine::scene {
namespace {

[[nodiscard]] nlohmann::json vec3_json(const core::Vec3& value) {
    nlohmann::json result = nlohmann::json::array();
    result.push_back(value.x);
    result.push_back(value.y);
    result.push_back(value.z);
    return result;
}

[[nodiscard]] nlohmann::json quat_json(const core::Quat& value) {
    nlohmann::json result = nlohmann::json::array();
    result.push_back(value.x);
    result.push_back(value.y);
    result.push_back(value.z);
    result.push_back(value.w);
    return result;
}

[[nodiscard]] nlohmann::json transform_json(const Transform& transform) {
    return {
        {"position", vec3_json(transform.position)},
        {"rotation", quat_json(transform.rotation)},
        {"scale", vec3_json(transform.scale)},
    };
}

} // namespace

nlohmann::json simulate(Scene& scene, int ticks, int tickRate) {
    const float delta_seconds = 1.0F / static_cast<float>(tickRate);

    for (int tick = 0; tick < ticks; ++tick) {
        for (const Entity entity : scene.entities()) {
            if (!scene.has<Motion>(entity)) {
                continue;
            }
            const Motion& motion = scene.component<Motion>(entity);
            Transform& transform = scene.transform(entity);
            transform.position = transform.position + (motion.linearVelocity * delta_seconds);
        }
    }

    nlohmann::json entities = nlohmann::json::array();
    for (const Entity entity : scene.entities()) {
        const auto id = scene.uuid(entity);
        if (!id.has_value()) {
            continue;
        }
        nlohmann::json document{
            {"id", id->to_string()},
            {"name", std::string(scene.name(entity))},
            {"transform", transform_json(scene.transform(entity))},
        };
        if (scene.has<Motion>(entity)) {
            const Motion& motion = scene.component<Motion>(entity);
            document["motion"] = {
                {"linearVelocity", vec3_json(motion.linearVelocity)},
            };
        }
        entities.push_back(std::move(document));
    }

    return {
        {"sceneId", scene.scene_id().to_string()},
        {"tickRate", tickRate},
        {"ticks", ticks},
        {"entities", std::move(entities)},
    };
}

} // namespace engine::scene
