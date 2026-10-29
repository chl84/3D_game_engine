#include "doctor.hpp"

#include <engine/core/version.hpp>
#include <engine/render/descriptor_requirements.hpp>

#include <volk.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace engine::cli {
namespace {

[[nodiscard]] std::string format_version(std::uint32_t version) {
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_VERSION_MINOR(version)) + "." +
           std::to_string(VK_VERSION_PATCH(version));
}

[[nodiscard]] std::string_view device_type_name(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return "other";
    }
}

[[nodiscard]] std::string lowercase(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] bool is_lavapipe(std::string_view device_name) {
    const std::string name = lowercase(device_name);
    return name.find("llvmpipe") != std::string::npos || name.find("lavapipe") != std::string::npos;
}

[[nodiscard]] nlohmann::json collect_vulkan() {
    nlohmann::json vulkan{
        {"loader", false},
        {"loaderVersion", "unknown"},
        {"instanceCreated", false},
        {"devices", nlohmann::json::array()},
        {"validationLayers", nlohmann::json::array()},
        {"lavapipe", false},
        {"descriptorIndexing", false},
    };

    if (volkInitialize() != VK_SUCCESS) {
        return vulkan;
    }

    vulkan["loader"] = true;
    const std::uint32_t loader_version = volkGetInstanceVersion();
    vulkan["loaderVersion"] = format_version(loader_version);

    std::vector<std::string> layers;
    std::uint32_t layer_count = 0;
    if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) == VK_SUCCESS &&
        layer_count > 0) {
        std::vector<VkLayerProperties> properties(layer_count);
        if (vkEnumerateInstanceLayerProperties(&layer_count, properties.data()) == VK_SUCCESS) {
            for (const auto& layer : properties) {
                layers.emplace_back(layer.layerName);
            }
        }
    }
    vulkan["validationLayers"] = layers;

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "engine-doctor";
    application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    application_info.pEngineName = "engine";
    application_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    const std::uint32_t instance_version =
        std::min(loader_version, static_cast<std::uint32_t>(VK_API_VERSION_1_3));
    application_info.apiVersion =
        instance_version >= VK_API_VERSION_1_1 ? instance_version : VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&create_info, nullptr, &instance) == VK_SUCCESS) {
        vulkan["instanceCreated"] = true;
        volkLoadInstance(instance);

        std::uint32_t device_count = 0;
        if (vkEnumeratePhysicalDevices(instance, &device_count, nullptr) == VK_SUCCESS &&
            device_count > 0) {
            std::vector<VkPhysicalDevice> devices(device_count);
            if (vkEnumeratePhysicalDevices(instance, &device_count, devices.data()) == VK_SUCCESS) {
                nlohmann::json device_list = nlohmann::json::array();
                bool lavapipe = false;
                bool descriptor_indexing = false;

                for (const VkPhysicalDevice device : devices) {
                    VkPhysicalDeviceProperties properties{};
                    vkGetPhysicalDeviceProperties(device, &properties);
                    const std::string name = properties.deviceName;
                    lavapipe = lavapipe || is_lavapipe(name);

                    const engine::render::DescriptorCapabilities capabilities =
                        instance_version >= VK_API_VERSION_1_2
                            ? engine::render::query_descriptor_capabilities(device)
                            : engine::render::DescriptorCapabilities{};
                    const engine::render::DescriptorRequirementReport requirement =
                        engine::render::evaluate_descriptor_requirements(capabilities);
                    descriptor_indexing = descriptor_indexing || requirement.supported;

                    device_list.push_back({
                        {"name", name},
                        {"type", std::string(device_type_name(properties.deviceType))},
                        {"apiVersion", format_version(properties.apiVersion)},
                        {"driverVersion", format_version(properties.driverVersion)},
                        {"vendorId", properties.vendorID},
                        {"deviceId", properties.deviceID},
                        {
                            "descriptorIndexing",
                            engine::render::descriptor_requirement_json(requirement),
                        },
                    });
                }

                vulkan["devices"] = std::move(device_list);
                vulkan["lavapipe"] = lavapipe;
                vulkan["descriptorIndexing"] = descriptor_indexing;
            }
        }

        vkDestroyInstance(instance, nullptr);
    }

    volkFinalize();
    return vulkan;
}

[[nodiscard]] nlohmann::json collect_project() {
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (error) {
        return {
            {"root", ""},
            {"writable", false},
        };
    }

    const auto write_mask = std::filesystem::perms::owner_write |
                            std::filesystem::perms::group_write |
                            std::filesystem::perms::others_write;
    const auto status = std::filesystem::status(current, error);
    const bool writable =
        !error && (status.permissions() & write_mask) != std::filesystem::perms::none;

    return {
        {"root", current.string()},
        {"writable", writable},
    };
}

[[nodiscard]] bool has_gpu_device(const nlohmann::json& devices) {
    return std::ranges::any_of(
        devices, [](const nlohmann::json& device) { return device.at("type") != "cpu"; });
}

[[nodiscard]] bool has_validation_layer(const nlohmann::json& layers) {
    return std::ranges::any_of(
        layers, [](const nlohmann::json& layer) { return layer == "VK_LAYER_KHRONOS_validation"; });
}

[[nodiscard]] std::string join(const nlohmann::json& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += ", ";
        }
        result += value.get<std::string>();
    }
    return result;
}

} // namespace

DoctorResult run_doctor(const DoctorOptions& options) {
    const nlohmann::json vulkan = collect_vulkan();
    const nlohmann::json project = collect_project();

    const bool loader = vulkan.at("loader").get<bool>();
    const bool instance_created = vulkan.at("instanceCreated").get<bool>();
    const bool has_devices = !vulkan.at("devices").empty();
    const bool has_gpu = has_gpu_device(vulkan.at("devices"));
    const bool has_layers = has_validation_layer(vulkan.at("validationLayers"));
    const bool has_lavapipe = vulkan.at("lavapipe").get<bool>();
    const bool has_descriptor_indexing = vulkan.at("descriptorIndexing").get<bool>();
    const bool project_writable = project.at("writable").get<bool>();

    nlohmann::json checks = nlohmann::json::array();
    const auto add_check = [&checks](std::string id, std::string status, std::string message) {
        checks.push_back({
            {"id", std::move(id)},
            {"status", std::move(status)},
            {"message", std::move(message)},
        });
    };

    add_check("compiler", "pass", std::string(engine::core::compiler()));

    if (loader && instance_created && has_devices && has_descriptor_indexing) {
        add_check("vulkan", "pass",
                  "Vulkan loader, device enumeration and descriptor indexing succeeded.");
    } else if (loader && instance_created && has_devices) {
        add_check("vulkan", "fail",
                  "Vulkan is available, but no device supports the required descriptor indexing "
                  "features.");
    } else if (loader) {
        add_check("vulkan", "warn", "Vulkan loader is present, but no device was enumerated.");
    } else {
        add_check("vulkan", "fail", "Vulkan loader is not available.");
    }

    if (has_descriptor_indexing) {
        add_check("descriptor-indexing", "pass",
                  "At least one device meets the required descriptor indexing features and "
                  "limits.");
    } else if (has_devices) {
        add_check("descriptor-indexing", "fail",
                  "No device meets the required descriptor indexing features and limits.");
    } else {
        add_check("descriptor-indexing", "warn",
                  "No Vulkan device is available to evaluate descriptor indexing.");
    }

    add_check("gpu", has_gpu ? "pass" : "warn",
              has_gpu ? "At least one hardware GPU is available." : "No hardware GPU was found.");

    add_check("validation-layers", has_layers ? "pass" : "warn",
              has_layers ? "VK_LAYER_KHRONOS_validation is available."
                         : "VK_LAYER_KHRONOS_validation is not available.");

    add_check("lavapipe", has_lavapipe ? "pass" : "warn",
              has_lavapipe ? "The lavapipe software device is available."
                           : "The lavapipe software device is not available.");

    add_check("project", project_writable ? "pass" : "fail",
              project_writable ? "The project root is writable."
                               : "The project root is not writable.");

    std::vector<std::string> failed_requirements;
    for (const auto& requirement : options.requirements) {
        if (requirement == "vulkan" &&
            !(loader && instance_created && has_devices && has_descriptor_indexing)) {
            failed_requirements.push_back(requirement);
        } else if (requirement == "gpu" && !has_gpu) {
            failed_requirements.push_back(requirement);
        } else if (requirement == "validation-layers" && !has_layers) {
            failed_requirements.push_back(requirement);
        } else if (requirement == "lavapipe" && !has_lavapipe) {
            failed_requirements.push_back(requirement);
        } else if (requirement != "vulkan" && requirement != "gpu" &&
                   requirement != "validation-layers" && requirement != "lavapipe") {
            failed_requirements.push_back(requirement);
        }
    }
    if (has_devices && !has_descriptor_indexing) {
        failed_requirements.emplace_back("descriptor-indexing");
    }

    nlohmann::json requirements{
        {"requested", options.requirements},
        {"failed", failed_requirements},
        {"passed", failed_requirements.empty()},
    };

    const nlohmann::json os{
        {"name", std::string(engine::core::os_name())},
        {"version", std::string(engine::core::os_version())},
    };

    DoctorResult result;
    result.report = {
        {"schemaVersion", 1},
        {"name", "engine"},
        {"version", std::string(engine::core::version())},
        {"gitCommit", std::string(engine::core::git_commit())},
        {"buildType", std::string(engine::core::build_type())},
        {"compiler", std::string(engine::core::compiler())},
        {"platform", std::string(engine::core::platform())},
        {"cxxStandard", std::string(engine::core::cxx_standard())},
        {"os", os},
        {"vulkan", vulkan},
        {"project", project},
        {"requirements", requirements},
        {"checks", checks},
    };
    result.failed_requirements = std::move(failed_requirements);
    return result;
}

void print_doctor_human(const nlohmann::json& report, std::ostream& out) {
    out << "engine " << report.at("version").get<std::string>() << " ("
        << report.at("gitCommit").get<std::string>() << ", "
        << report.at("buildType").get<std::string>() << ")\n";
    out << "OS: " << report.at("os").at("name").get<std::string>() << ' '
        << report.at("os").at("version").get<std::string>() << '\n';
    out << "Compiler: " << report.at("compiler").get<std::string>() << '\n';
    out << "Project: " << report.at("project").at("root").get<std::string>() << " ("
        << (report.at("project").at("writable").get<bool>() ? "writable" : "not writable") << ")\n";

    const auto& vulkan = report.at("vulkan");
    out << "Vulkan loader: " << (vulkan.at("loader").get<bool>() ? "yes" : "no") << " ("
        << vulkan.at("loaderVersion").get<std::string>() << ")\n";
    out << "Vulkan instance: "
        << (vulkan.at("instanceCreated").get<bool>() ? "created" : "not created") << '\n';

    out << "Devices:\n";
    if (vulkan.at("devices").empty()) {
        out << "  (none)\n";
    } else {
        for (const auto& device : vulkan.at("devices")) {
            out << "  - " << device.at("name").get<std::string>() << " ("
                << device.at("type").get<std::string>() << ", API "
                << device.at("apiVersion").get<std::string>() << ", driver "
                << device.at("driverVersion").get<std::string>() << ")\n";
        }
    }

    out << "Validation layers: " << join(vulkan.at("validationLayers")) << '\n';
    out << "Lavapipe: " << (vulkan.at("lavapipe").get<bool>() ? "yes" : "no") << '\n';
    out << "Descriptor indexing: " << (vulkan.at("descriptorIndexing").get<bool>() ? "yes" : "no")
        << '\n';

    out << "Checks:\n";
    for (const auto& check : report.at("checks")) {
        out << "  [" << check.at("status").get<std::string>() << "] "
            << check.at("id").get<std::string>() << ": " << check.at("message").get<std::string>()
            << '\n';
    }

    const auto& requirements = report.at("requirements");
    if (!requirements.at("requested").empty()) {
        out << "Required checks: " << (requirements.at("passed").get<bool>() ? "passed" : "failed")
            << '\n';
        if (!requirements.at("failed").empty()) {
            out << "  failed: " << join(requirements.at("failed")) << '\n';
        }
    } else if (!requirements.at("failed").empty()) {
        out << "Engine requirements failed: " << join(requirements.at("failed")) << '\n';
    }
}

} // namespace engine::cli
