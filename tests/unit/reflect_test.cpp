#include <engine/core/reflect.hpp>

#include <catch2/catch_test_macros.hpp>

using engine::core::FieldType;
using engine::core::Quat;
using engine::core::ReflectionRegistry;
using engine::core::Vec3;

namespace {

struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct Camera {
    float fovYRadians{1.0471975511965976F};
    float near{0.1F};
    float far{1000.0F};
    bool enabled{true};
};

[[nodiscard]] ReflectionRegistry make_registry() {
    ReflectionRegistry registry;

    registry.component<Transform>("engine.scene.Transform")
        .field("position", &Transform::position, "Local position")
        .field("rotation", &Transform::rotation, "Local rotation")
        .field("scale", &Transform::scale, "Local scale")
        .build();

    registry.component<Camera>("engine.scene.Camera")
        .field("fovYRadians", &Camera::fovYRadians)
        .field("near", &Camera::near)
        .field("far", &Camera::far)
        .field("enabled", &Camera::enabled)
        .build();

    return registry;
}

} // namespace

TEST_CASE("reflection registry exposes sorted component names and fields", "[reflect]") {
    const ReflectionRegistry registry = make_registry();

    const auto names = registry.names();
    REQUIRE(names == std::vector<std::string>{"engine.scene.Camera", "engine.scene.Transform"});

    const auto* transform = registry.find("engine.scene.Transform");
    REQUIRE(transform != nullptr);
    REQUIRE(transform->name() == "engine.scene.Transform");
    REQUIRE(transform->fields().size() == 3);
    REQUIRE(transform->find_field("position") != nullptr);
    REQUIRE(transform->find_field("position")->type == FieldType::Vec3);
    REQUIRE(transform->find_field("rotation")->type == FieldType::Quat);
    REQUIRE(transform->find_field("missing") == nullptr);
}

TEST_CASE("reflection serializes and parses components", "[reflect]") {
    const ReflectionRegistry registry = make_registry();
    const auto* transform = registry.find("engine.scene.Transform");
    REQUIRE(transform != nullptr);

    Transform value;
    value.position = {1.0F, 2.0F, 3.0F};
    value.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
    value.scale = {4.0F, 5.0F, 6.0F};

    const auto json = transform->to_json(&value);
    REQUIRE(json.at("position") == nlohmann::json::array({1.0F, 2.0F, 3.0F}));
    REQUIRE(json.at("rotation") == nlohmann::json::array({0.0F, 0.0F, 0.0F, 1.0F}));
    REQUIRE(json.at("scale") == nlohmann::json::array({4.0F, 5.0F, 6.0F}));

    Transform parsed;
    const auto result = transform->from_json(&parsed, json);
    REQUIRE(result.has_value());
    REQUIRE(parsed.position == value.position);
    REQUIRE(parsed.rotation == value.rotation);
    REQUIRE(parsed.scale == value.scale);
}

TEST_CASE("reflection supports partial component updates", "[reflect]") {
    const ReflectionRegistry registry = make_registry();
    const auto* transform = registry.find("engine.scene.Transform");
    REQUIRE(transform != nullptr);

    Transform value;
    value.position = {1.0F, 2.0F, 3.0F};
    value.scale = {4.0F, 5.0F, 6.0F};

    const auto result = transform->from_json(&value, {{"position", {7.0F, 8.0F, 9.0F}}});
    REQUIRE(result.has_value());
    REQUIRE(value.position == Vec3{7.0F, 8.0F, 9.0F});
    REQUIRE(value.scale == Vec3{4.0F, 5.0F, 6.0F});
}

TEST_CASE("reflection rejects unknown fields and wrong types", "[reflect]") {
    const ReflectionRegistry registry = make_registry();
    const auto* transform = registry.find("engine.scene.Transform");
    REQUIRE(transform != nullptr);

    Transform value;
    auto unknown = transform->from_json(&value, {{"unknown", 1}});
    REQUIRE_FALSE(unknown.has_value());
    REQUIRE(unknown.error().code == engine::core::ErrorCode::Validation);
    REQUIRE(unknown.error().details.at("field") == "unknown");

    auto wrong_type = transform->from_json(&value, {{"position", "not an array"}});
    REQUIRE_FALSE(wrong_type.has_value());
    REQUIRE(wrong_type.error().code == engine::core::ErrorCode::Validation);
    REQUIRE(wrong_type.error().details.at("field") == "position");
}

TEST_CASE("reflection generates draft-07 component schemas", "[reflect]") {
    const ReflectionRegistry registry = make_registry();

    const auto schema = registry.schema("engine.scene.Transform");
    REQUIRE(schema.has_value());
    REQUIRE(schema->at("$schema") == "http://json-schema.org/draft-07/schema#");
    REQUIRE(schema->at("type") == "object");
    REQUIRE(schema->at("additionalProperties") == false);
    REQUIRE(schema->at("required") == nlohmann::json::array({"position", "rotation", "scale"}));
    REQUIRE(schema->at("properties").at("position").at("type") == "array");
    REQUIRE(schema->at("properties").at("position").at("minItems") == 3);
    REQUIRE(schema->at("properties").at("position").at("maxItems") == 3);
    REQUIRE(schema->at("properties").at("rotation").at("minItems") == 4);

    const auto schemas = registry.schemas();
    REQUIRE(schemas.contains("engine.scene.Camera"));
    REQUIRE(schemas.contains("engine.scene.Transform"));

    const auto missing = registry.schema("engine.scene.Missing");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == engine::core::ErrorCode::NotFound);
}
