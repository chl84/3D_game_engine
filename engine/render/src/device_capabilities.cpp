#include <engine/render/descriptor_requirements.hpp>

#include <utility>

namespace engine::render {
namespace {

[[nodiscard]] bool supports_vulkan_12(std::uint32_t version) {
    return VK_API_VERSION_MAJOR(version) > 1 ||
           (VK_API_VERSION_MAJOR(version) == 1 && VK_API_VERSION_MINOR(version) >= 2);
}

void require_feature(std::vector<std::string>& missing, bool supported, const char* name) {
    if (!supported) {
        missing.emplace_back(name);
    }
}

void require_limit(std::vector<DescriptorLimitRequirement>& missing, const char* name,
                   std::uint32_t required, std::uint32_t supported) {
    if (supported < required) {
        missing.push_back({name, required, supported});
    }
}

} // namespace

DescriptorCapabilities query_descriptor_capabilities(VkPhysicalDevice device) noexcept {
    DescriptorCapabilities capabilities;
    if (device == VK_NULL_HANDLE || vkGetPhysicalDeviceFeatures2 == nullptr ||
        vkGetPhysicalDeviceProperties2 == nullptr) {
        return capabilities;
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    if (!supports_vulkan_12(properties.apiVersion)) {
        return capabilities;
    }

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(device, &features);

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPhysicalDeviceVulkan12Properties properties12{};
    properties12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &properties12;
    vkGetPhysicalDeviceProperties2(device, &properties2);

    capabilities.descriptor_indexing = features12.descriptorIndexing == VK_TRUE;
    capabilities.shader_sampled_image_array_non_uniform_indexing =
        features12.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
    capabilities.descriptor_binding_partially_bound =
        features12.descriptorBindingPartiallyBound == VK_TRUE;
    capabilities.descriptor_binding_variable_descriptor_count =
        features12.descriptorBindingVariableDescriptorCount == VK_TRUE;
    capabilities.descriptor_binding_sampled_image_update_after_bind =
        features12.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE;
    capabilities.runtime_descriptor_array = features12.runtimeDescriptorArray == VK_TRUE;
    capabilities.max_per_stage_descriptor_update_after_bind_sampled_images =
        properties12.maxPerStageDescriptorUpdateAfterBindSampledImages;
    capabilities.max_descriptor_set_update_after_bind_sampled_images =
        properties12.maxDescriptorSetUpdateAfterBindSampledImages;
    capabilities.max_update_after_bind_descriptors_in_all_pools =
        properties12.maxUpdateAfterBindDescriptorsInAllPools;
    capabilities.max_bound_descriptor_sets = properties2.properties.limits.maxBoundDescriptorSets;
    capabilities.buffer_device_address = features12.bufferDeviceAddress == VK_TRUE;
    return capabilities;
}

DescriptorRequirementReport
evaluate_descriptor_requirements(const DescriptorCapabilities& capabilities) {
    DescriptorRequirementReport report;
    require_feature(report.missing_features, capabilities.descriptor_indexing,
                    "descriptorIndexing");
    require_feature(report.missing_features,
                    capabilities.shader_sampled_image_array_non_uniform_indexing,
                    "shaderSampledImageArrayNonUniformIndexing");
    require_feature(report.missing_features, capabilities.descriptor_binding_partially_bound,
                    "descriptorBindingPartiallyBound");
    require_feature(report.missing_features,
                    capabilities.descriptor_binding_variable_descriptor_count,
                    "descriptorBindingVariableDescriptorCount");
    require_feature(report.missing_features,
                    capabilities.descriptor_binding_sampled_image_update_after_bind,
                    "descriptorBindingSampledImageUpdateAfterBind");
    require_feature(report.missing_features, capabilities.runtime_descriptor_array,
                    "runtimeDescriptorArray");

    require_limit(report.missing_limits, "maxPerStageDescriptorUpdateAfterBindSampledImages",
                  kBindlessSampledImageBudget,
                  capabilities.max_per_stage_descriptor_update_after_bind_sampled_images);
    require_limit(report.missing_limits, "maxDescriptorSetUpdateAfterBindSampledImages",
                  kBindlessSampledImageBudget,
                  capabilities.max_descriptor_set_update_after_bind_sampled_images);
    require_limit(report.missing_limits, "maxUpdateAfterBindDescriptorsInAllPools",
                  kBindlessDescriptorBudget,
                  capabilities.max_update_after_bind_descriptors_in_all_pools);
    require_limit(report.missing_limits, "maxBoundDescriptorSets", kMinimumBoundDescriptorSets,
                  capabilities.max_bound_descriptor_sets);

    report.supported = report.missing_features.empty() && report.missing_limits.empty();
    return report;
}

nlohmann::json descriptor_requirement_json(const DescriptorRequirementReport& report) {
    nlohmann::json missing_limits = nlohmann::json::array();
    for (const DescriptorLimitRequirement& limit : report.missing_limits) {
        missing_limits.push_back({
            {"name", limit.name},
            {"required", limit.required},
            {"supported", limit.supported},
        });
    }

    nlohmann::json value{
        {"supported", report.supported},
        {"missingFeatures", report.missing_features},
        {"missingLimits", std::move(missing_limits)},
    };
    if (!report.supported) {
        value["errorCode"] = "E_UNSUPPORTED";
    }
    return value;
}

core::Error make_descriptor_requirement_error(const DescriptorRequirementReport& report) {
    nlohmann::json details = descriptor_requirement_json(report);
    details["stage"] = "descriptor-indexing";
    return core::make_error(
        core::ErrorCode::Unsupported,
        "the Vulkan device does not meet the required descriptor indexing capabilities",
        "Use a Vulkan 1.3 driver with descriptor indexing, update-after-bind and at least 4096 "
        "sampled images.",
        std::move(details));
}

} // namespace engine::render
