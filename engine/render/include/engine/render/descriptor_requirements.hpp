#pragma once

#include <engine/core/error.hpp>

#include <volk.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace engine::render {

// Engine budgets for the bindless descriptor path. These are engine limits,
// not driver limits. The texture budget is a sub-budget of the total GPU
// memory budget.
inline constexpr std::uint32_t kBindlessSampledImageBudget = 4096;
inline constexpr std::uint32_t kBindlessSamplerBudget = 2;
inline constexpr std::uint32_t kBindlessDescriptorBudget =
    kBindlessSampledImageBudget + kBindlessSamplerBudget;
inline constexpr std::uint32_t kBindlessMaterialBudget = 4096;
inline constexpr std::uint32_t kMinimumBoundDescriptorSets = 2;
inline constexpr std::uint64_t kTextureMemoryBudgetBytes = 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kTotalGpuMemoryBudgetBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;

// The descriptor-indexing capabilities required by the engine. The engine has
// no classic descriptor-set fallback; a device that lacks any required feature
// or limit is rejected.
struct DescriptorCapabilities {
    bool descriptor_indexing{false};
    bool shader_sampled_image_array_non_uniform_indexing{false};
    bool descriptor_binding_partially_bound{false};
    bool descriptor_binding_variable_descriptor_count{false};
    bool descriptor_binding_sampled_image_update_after_bind{false};
    bool runtime_descriptor_array{false};
    std::uint32_t max_per_stage_descriptor_update_after_bind_sampled_images{0};
    std::uint32_t max_descriptor_set_update_after_bind_sampled_images{0};
    std::uint32_t max_update_after_bind_descriptors_in_all_pools{0};
    std::uint32_t max_bound_descriptor_sets{0};
    // Queried for future use; buffer device address is not required in M3.
    bool buffer_device_address{false};
};

struct DescriptorLimitRequirement {
    std::string name;
    std::uint32_t required{0};
    std::uint32_t supported{0};
};

struct DescriptorRequirementReport {
    bool supported{false};
    std::vector<std::string> missing_features;
    std::vector<DescriptorLimitRequirement> missing_limits;
};

// Queries the Vulkan 1.2 descriptor-indexing features and limits. Returns a
// zeroed value when the device or the required query functions are
// unavailable.
[[nodiscard]] DescriptorCapabilities
query_descriptor_capabilities(VkPhysicalDevice device) noexcept;

[[nodiscard]] DescriptorRequirementReport
evaluate_descriptor_requirements(const DescriptorCapabilities& capabilities);

// Serializes a requirement report for doctor output and error details. The
// result contains supported, missingFeatures and missingLimits; an unsupported
// report also carries errorCode = E_UNSUPPORTED.
[[nodiscard]] nlohmann::json descriptor_requirement_json(const DescriptorRequirementReport& report);

// Builds the machine-readable error used by device creation and the doctor
// requirement path. The report must describe an unsupported device.
[[nodiscard]] core::Error
make_descriptor_requirement_error(const DescriptorRequirementReport& report);

} // namespace engine::render
