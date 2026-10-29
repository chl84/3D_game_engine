#pragma once

#include <nlohmann/json.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace engine::cli {

struct DoctorOptions {
    std::vector<std::string> requirements;
};

struct DoctorResult {
    nlohmann::json report;
    std::vector<std::string> failed_requirements;
    // True when an enumerated device exists but the engine's mandatory
    // descriptor-indexing requirement is not met. A machine with no Vulkan
    // device at all remains a diagnostic result unless --require is used.
    bool engine_requirements_failed{false};
};

[[nodiscard]] DoctorResult run_doctor(const DoctorOptions& options);

void print_doctor_human(const nlohmann::json& report, std::ostream& out);

} // namespace engine::cli
