#include "doctor.hpp"

#include <engine/core/error.hpp>
#include <engine/core/version.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string json_quote(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char c : value) {
        switch (c) {
        case '"':
        case '\\':
            out.push_back('\\');
            out.push_back(c);
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    out.push_back('"');
    return out;
}

void print_version_json() {
    std::cout << '{' << "\"name\":" << json_quote("engine") << ','
              << "\"version\":" << json_quote(engine::core::version()) << ','
              << "\"gitCommit\":" << json_quote(engine::core::git_commit()) << ','
              << "\"buildType\":" << json_quote(engine::core::build_type()) << ','
              << "\"compiler\":" << json_quote(engine::core::compiler()) << ','
              << "\"platform\":" << json_quote(engine::core::platform()) << ','
              << "\"cxxStandard\":" << json_quote(engine::core::cxx_standard()) << "}\n";
}

void print_version_human() {
    std::cout << "engine " << engine::core::version() << " (" << engine::core::git_commit() << ", "
              << engine::core::build_type() << ", " << engine::core::compiler() << ", "
              << engine::core::platform() << ")\n";
}

bool valid_requirement(std::string_view requirement) {
    constexpr std::array<std::string_view, 4> kRequirements{
        "vulkan",
        "gpu",
        "validation-layers",
        "lavapipe",
    };
    return std::ranges::any_of(
        kRequirements, [requirement](std::string_view value) { return requirement == value; });
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"AI-first 3D game engine", "engine"};
    app.set_help_flag("--help,-h", "Print help.");

    bool json = false;
    app.add_flag("--json", json, "Emit a machine-readable JSON document.");

    bool show_version = false;
    app.add_flag("--version,-v", show_version, "Print the engine version.");

    auto* doctor = app.add_subcommand("doctor", "Report environment and Vulkan capabilities.");
    bool doctor_json = false;
    doctor->add_flag("--json", doctor_json, "Emit a machine-readable JSON document.");

    std::vector<std::string> requirements;
    doctor
        ->add_option("--require", requirements,
                     "Required check: vulkan, gpu, validation-layers or lavapipe.")
        ->expected(0, 4);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        const int code = app.exit(error);
        return code == 0 ? 0 : 2;
    }

    if (show_version && *doctor) {
        std::cerr << "error: --version and the doctor command cannot be combined\n";
        return 2;
    }

    if (show_version) {
        if (json) {
            print_version_json();
        } else {
            print_version_human();
        }
        return 0;
    }

    if (*doctor) {
        const auto invalid_requirement =
            std::ranges::find_if(requirements, [](const std::string& requirement) {
                return !valid_requirement(requirement);
            });
        if (invalid_requirement != requirements.end()) {
            std::cerr << "error: unknown requirement: " << *invalid_requirement << '\n';
            return 2;
        }

        const auto result = engine::cli::run_doctor({requirements});
        if (json || doctor_json) {
            std::cout << result.report.dump(2) << '\n';
        } else {
            engine::cli::print_doctor_human(result.report, std::cout);
        }

        return result.failed_requirements.empty()
                   ? 0
                   : engine::core::exit_code(engine::core::ErrorCode::Precondition);
    }

    std::cerr << "error: no command\n" << app.help() << '\n';
    return 2;
}
