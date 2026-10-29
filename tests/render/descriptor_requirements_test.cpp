#include <engine/render/descriptor_requirements.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

[[nodiscard]] engine::render::DescriptorCapabilities supported_capabilities() {
    engine::render::DescriptorCapabilities capabilities;
    capabilities.descriptor_indexing = true;
    capabilities.shader_sampled_image_array_non_uniform_indexing = true;
    capabilities.descriptor_binding_partially_bound = true;
    capabilities.descriptor_binding_variable_descriptor_count = true;
    capabilities.descriptor_binding_sampled_image_update_after_bind = true;
    capabilities.runtime_descriptor_array = true;
    capabilities.max_per_stage_descriptor_update_after_bind_sampled_images =
        engine::render::kBindlessSampledImageBudget;
    capabilities.max_descriptor_set_update_after_bind_sampled_images =
        engine::render::kBindlessSampledImageBudget;
    capabilities.max_update_after_bind_descriptors_in_all_pools =
        engine::render::kBindlessDescriptorBudget;
    capabilities.max_bound_descriptor_sets = engine::render::kMinimumBoundDescriptorSets;
    capabilities.buffer_device_address = true;
    return capabilities;
}

} // namespace

TEST_CASE("descriptor requirements accept the engine minimum", "[render][descriptor]") {
    const auto report = engine::render::evaluate_descriptor_requirements(supported_capabilities());
    CHECK(report.supported);
    CHECK(report.missing_features.empty());
    CHECK(report.missing_limits.empty());

    const nlohmann::json document = engine::render::descriptor_requirement_json(report);
    CHECK(document.at("supported").get<bool>());
    CHECK_FALSE(document.contains("errorCode"));
    CHECK(document.at("missingFeatures").empty());
    CHECK(document.at("missingLimits").empty());
}

TEST_CASE("a missing descriptor feature produces a clear error", "[render][descriptor]") {
    engine::render::DescriptorCapabilities capabilities = supported_capabilities();
    capabilities.descriptor_indexing = false;

    const auto report = engine::render::evaluate_descriptor_requirements(capabilities);
    REQUIRE_FALSE(report.supported);
    REQUIRE(report.missing_features.size() == 1);
    CHECK(report.missing_features.front() == "descriptorIndexing");
    CHECK(report.missing_limits.empty());

    const nlohmann::json document = engine::render::descriptor_requirement_json(report);
    CHECK(document.at("supported").get<bool>() == false);
    CHECK(document.at("errorCode").get<std::string>() == "E_UNSUPPORTED");
    CHECK(document.at("missingFeatures").at(0).get<std::string>() == "descriptorIndexing");

    const engine::core::Error error = engine::render::make_descriptor_requirement_error(report);
    CHECK(error.code == engine::core::ErrorCode::Unsupported);
    CHECK(error.details.at("stage").get<std::string>() == "descriptor-indexing");
    CHECK(error.details.at("errorCode").get<std::string>() == "E_UNSUPPORTED");
    REQUIRE(error.details.at("missingFeatures").size() == 1);
    CHECK(error.details.at("missingFeatures").at(0).get<std::string>() == "descriptorIndexing");
    CHECK(error.details.at("missingLimits").empty());
}

TEST_CASE("a low descriptor limit produces a clear error", "[render][descriptor]") {
    engine::render::DescriptorCapabilities capabilities = supported_capabilities();
    capabilities.max_per_stage_descriptor_update_after_bind_sampled_images =
        engine::render::kBindlessSampledImageBudget - 1;

    const auto report = engine::render::evaluate_descriptor_requirements(capabilities);
    REQUIRE_FALSE(report.supported);
    CHECK(report.missing_features.empty());
    REQUIRE(report.missing_limits.size() == 1);
    CHECK(report.missing_limits.front().name ==
          "maxPerStageDescriptorUpdateAfterBindSampledImages");
    CHECK(report.missing_limits.front().required == engine::render::kBindlessSampledImageBudget);
    CHECK(report.missing_limits.front().supported ==
          engine::render::kBindlessSampledImageBudget - 1);

    const engine::core::Error error = engine::render::make_descriptor_requirement_error(report);
    CHECK(error.code == engine::core::ErrorCode::Unsupported);
    REQUIRE(error.details.at("missingLimits").size() == 1);
    const nlohmann::json& limit = error.details.at("missingLimits").at(0);
    CHECK(limit.at("name").get<std::string>() ==
          "maxPerStageDescriptorUpdateAfterBindSampledImages");
    CHECK(limit.at("required").get<std::uint32_t>() == engine::render::kBindlessSampledImageBudget);
    CHECK(limit.at("supported").get<std::uint32_t>() ==
          engine::render::kBindlessSampledImageBudget - 1);
}
