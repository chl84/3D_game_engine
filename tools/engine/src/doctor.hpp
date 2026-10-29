#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

namespace engine::cli {

struct DoctorOptions {
    std::vector<std::string> requirements;
};

// At least one device must support the mandatory descriptor-indexing path.
// A machine with no devices is diagnostic-only; descriptor-indexing is not a
// failure until a device exists and none of them supports the requirement.
[[nodiscard]] inline bool any_device_supports_descriptor_indexing(const nlohmann::json& devices) {
    return std::ranges::any_of(devices, [](const nlohmann::json& device) {
        if (!device.contains("descriptorIndexing") ||
            !device.at("descriptorIndexing").is_object()) {
            return false;
        }
        return device.at("descriptorIndexing").value("supported", false);
    });
}

[[nodiscard]] inline bool descriptor_indexing_requirement_failed(const nlohmann::json& devices) {
    return !devices.empty() && !any_device_supports_descriptor_indexing(devices);
}

struct DoctorResult {
    nlohmann::json report;
    std::vector<std::string> failed_requirements;
};

[[nodiscard]] DoctorResult run_doctor(const DoctorOptions& options);

void print_doctor_human(const nlohmann::json& report, std::ostream& out);

} // namespace engine::cli
