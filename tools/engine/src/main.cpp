#include "commands.hpp"
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
    bool dry_run = false;
    app.add_flag("--json", json, "Emit a machine-readable JSON document.");

    bool show_version = false;
    app.add_flag("--version,-v", show_version, "Print the engine version.");

    auto* doctor = app.add_subcommand("doctor", "Report environment and Vulkan capabilities.");
    bool doctor_json = false;
    doctor->add_flag("--json", doctor_json, "Emit a machine-readable JSON document.");

    std::vector<std::string> requirements;
    doctor
        ->add_option("--require", requirements,
                     "Required check: vulkan (includes descriptor indexing), gpu, "
                     "validation-layers or lavapipe.")
        ->expected(0, 4);

    auto* project_command = app.add_subcommand("project", "Create, validate or unlock a project.");
    auto* project_create = project_command->add_subcommand("create", "Create a new project.");
    std::string project_path;
    std::string project_name = "Untitled";
    project_create->add_option("path", project_path, "Project directory.")->required();
    project_create->add_option("--name", project_name, "Project name.");

    auto* project_validate = project_command->add_subcommand("validate", "Validate a project.");
    std::string validate_path;
    project_validate->add_option("path", validate_path, "Project directory.")->required();

    auto* project_unlock =
        project_command->add_subcommand("unlock", "Remove a stale project lock.");
    std::string unlock_path;
    bool unlock_force = false;
    project_unlock->add_option("path", unlock_path, "Project directory.")->required();
    project_unlock->add_flag("--force", unlock_force, "Remove the lock without prompting.");
    project_command->require_subcommand(1);

    auto* scene_command = app.add_subcommand("scene", "Create, inspect or validate a scene.");
    auto* scene_create = scene_command->add_subcommand("create", "Create an empty scene.");
    std::string scene_project;
    std::string scene_name = "main";
    std::string scene_relative = "scenes/main.scene.json";
    scene_create->add_option("project", scene_project, "Project directory.")->required();
    scene_create->add_option("--name", scene_name, "Scene name.");
    scene_create->add_option("--path", scene_relative, "Scene path inside the project.");

    auto* scene_inspect = scene_command->add_subcommand("inspect", "Print a scene as JSON.");
    std::string inspect_project;
    std::string inspect_scene = "scenes/main.scene.json";
    scene_inspect->add_option("project", inspect_project, "Project directory.")->required();
    scene_inspect->add_option("scene", inspect_scene, "Scene path inside the project.");

    auto* scene_validate = scene_command->add_subcommand("validate", "Validate a scene.");
    std::string validate_scene_project;
    std::string validate_scene_path = "scenes/main.scene.json";
    scene_validate->add_option("project", validate_scene_project, "Project directory.")->required();
    scene_validate->add_option("scene", validate_scene_path, "Scene path inside the project.");
    scene_command->require_subcommand(1);

    auto* entity_command = app.add_subcommand("entity", "Create, delete or reparent an entity.");
    auto* entity_create = entity_command->add_subcommand("create", "Create an entity.");
    std::string entity_project;
    std::string entity_scene = "scenes/main.scene.json";
    std::string entity_name;
    std::string entity_parent;
    std::string entity_id;
    entity_create->add_option("project", entity_project, "Project directory.")->required();
    entity_create->add_option("scene", entity_scene, "Scene path inside the project.");
    entity_create->add_option("--name", entity_name, "Entity name.")->required();
    entity_create->add_option("--parent", entity_parent, "Parent UUID, or null.");
    entity_create->add_option("--id", entity_id, "Explicit entity UUID.");

    auto* entity_delete = entity_command->add_subcommand("delete", "Delete an entity subtree.");
    std::string delete_project;
    std::string delete_scene = "scenes/main.scene.json";
    std::string delete_id;
    entity_delete->add_option("project", delete_project, "Project directory.")->required();
    entity_delete->add_option("scene", delete_scene, "Scene path inside the project.");
    entity_delete->add_option("--id", delete_id, "Entity UUID.")->required();

    auto* entity_reparent = entity_command->add_subcommand("reparent", "Change an entity parent.");
    std::string reparent_project;
    std::string reparent_scene = "scenes/main.scene.json";
    std::string reparent_id;
    std::string reparent_parent;
    entity_reparent->add_option("project", reparent_project, "Project directory.")->required();
    entity_reparent->add_option("scene", reparent_scene, "Scene path inside the project.");
    entity_reparent->add_option("--id", reparent_id, "Entity UUID.")->required();
    entity_reparent->add_option("--parent", reparent_parent, "Parent UUID, or null.")->required();
    entity_command->require_subcommand(1);

    auto* component_command =
        app.add_subcommand("component", "Set, add or remove a component on an entity.");
    auto* component_set = component_command->add_subcommand("set", "Set component fields.");
    std::string set_project;
    std::string set_scene = "scenes/main.scene.json";
    std::string set_id;
    std::string set_component;
    std::string set_value;
    component_set->add_option("project", set_project, "Project directory.")->required();
    component_set->add_option("scene", set_scene, "Scene path inside the project.");
    component_set->add_option("--id", set_id, "Entity UUID.")->required();
    component_set->add_option("--component", set_component, "Component name.")->required();
    component_set->add_option("--value", set_value, "JSON object with fields.")->required();

    auto* component_add = component_command->add_subcommand("add", "Add a component.");
    std::string add_project;
    std::string add_scene = "scenes/main.scene.json";
    std::string add_id;
    std::string add_component;
    component_add->add_option("project", add_project, "Project directory.")->required();
    component_add->add_option("scene", add_scene, "Scene path inside the project.");
    component_add->add_option("--id", add_id, "Entity UUID.")->required();
    component_add->add_option("--component", add_component, "Component name.")->required();

    auto* component_remove = component_command->add_subcommand("remove", "Remove a component.");
    std::string remove_project;
    std::string remove_scene = "scenes/main.scene.json";
    std::string remove_id;
    std::string remove_component;
    component_remove->add_option("project", remove_project, "Project directory.")->required();
    component_remove->add_option("scene", remove_scene, "Scene path inside the project.");
    component_remove->add_option("--id", remove_id, "Entity UUID.")->required();
    component_remove->add_option("--component", remove_component, "Component name.")->required();
    component_command->require_subcommand(1);

    auto* schema_command = app.add_subcommand("schema", "Print component schemas.");
    schema_command->add_flag("--json", json, "Emit a machine-readable JSON document.");

    auto* simulate_command = app.add_subcommand("simulate", "Run fixed simulation ticks.");
    std::string simulate_project;
    std::string simulate_scene = "scenes/main.scene.json";
    int simulate_ticks = 60;
    simulate_command->add_option("project", simulate_project, "Project directory.")->required();
    simulate_command->add_option("scene", simulate_scene, "Scene path inside the project.");
    simulate_command->add_option("--ticks", simulate_ticks, "Number of ticks.")
        ->check(CLI::PositiveNumber);

    auto* batch_command = app.add_subcommand("batch", "Execute a batch of commands.");
    std::string batch_project;
    std::string batch_scene = "scenes/main.scene.json";
    std::string batch_file;
    int batch_undo = 0;
    batch_command->add_option("project", batch_project, "Project directory.")->required();
    batch_command->add_option("scene", batch_scene, "Scene path inside the project.");
    batch_command->add_option("--file", batch_file, "Batch JSON file.")->required();
    batch_command->add_option("--undo", batch_undo, "Undo the last N commands before saving.")
        ->check(CLI::NonNegativeNumber);

    auto* screenshot_command = app.add_subcommand("screenshot", "Render a scene to a PNG file.");
    std::string screenshot_project;
    std::string screenshot_scene = "scenes/main.scene.json";
    std::string screenshot_out;
    int screenshot_frames = 1;
    int screenshot_width = 1280;
    int screenshot_height = 720;
    screenshot_command->add_option("project", screenshot_project, "Project directory.")->required();
    screenshot_command->add_option("scene", screenshot_scene, "Scene path inside the project.");
    screenshot_command->add_option("--out", screenshot_out, "Output PNG file inside the project.")
        ->required();
    screenshot_command->add_option("--frames", screenshot_frames, "Frames to render.")
        ->check(CLI::PositiveNumber);
    screenshot_command->add_option("--width", screenshot_width, "Image width.")
        ->check(CLI::PositiveNumber);
    screenshot_command->add_option("--height", screenshot_height, "Image height.")
        ->check(CLI::PositiveNumber);

    for (CLI::App* command : {
             project_create,
             project_validate,
             project_unlock,
             scene_create,
             scene_inspect,
             scene_validate,
             entity_create,
             entity_delete,
             entity_reparent,
             component_set,
             component_add,
             component_remove,
             simulate_command,
             batch_command,
             screenshot_command,
         }) {
        command->add_flag("--json", json, "Emit a machine-readable JSON document.");
    }
    for (CLI::App* command : {
             project_create,
             project_unlock,
             scene_create,
             entity_create,
             entity_delete,
             entity_reparent,
             component_set,
             component_add,
             component_remove,
             batch_command,
         }) {
        command->add_flag("--dry-run", dry_run, "Validate without writing any files.");
    }

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

        return result.failed_requirements.empty() && !result.engine_requirements_failed
                   ? 0
                   : engine::core::exit_code(engine::core::ErrorCode::Precondition);
    }

    if (*project_command) {
        if (*project_create) {
            return engine::cli::project_create(project_path, project_name, json, dry_run);
        }
        if (*project_validate) {
            return engine::cli::project_validate(validate_path, json);
        }
        if (*project_unlock) {
            return engine::cli::project_unlock(unlock_path, unlock_force, json, dry_run);
        }
    }

    if (*scene_command) {
        if (*scene_create) {
            return engine::cli::scene_create(scene_project, scene_name, scene_relative, json,
                                             dry_run);
        }
        if (*scene_inspect) {
            return engine::cli::scene_inspect(inspect_project, inspect_scene, json);
        }
        if (*scene_validate) {
            return engine::cli::scene_validate(validate_scene_project, validate_scene_path, json);
        }
    }

    if (*entity_command) {
        if (*entity_create) {
            return engine::cli::entity_create(entity_project, entity_scene, entity_name,
                                              entity_parent, entity_id, json, dry_run);
        }
        if (*entity_delete) {
            return engine::cli::entity_delete(delete_project, delete_scene, delete_id, json,
                                              dry_run);
        }
        if (*entity_reparent) {
            return engine::cli::entity_reparent(reparent_project, reparent_scene, reparent_id,
                                                reparent_parent, json, dry_run);
        }
    }

    if (*component_command) {
        if (*component_set) {
            return engine::cli::component_set(set_project, set_scene, set_id, set_component,
                                              set_value, json, dry_run);
        }
        if (*component_add) {
            return engine::cli::component_add(add_project, add_scene, add_id, add_component, json,
                                              dry_run);
        }
        if (*component_remove) {
            return engine::cli::component_remove(remove_project, remove_scene, remove_id,
                                                 remove_component, json, dry_run);
        }
    }

    if (*schema_command) {
        return engine::cli::schema(json);
    }

    if (*simulate_command) {
        return engine::cli::simulate(simulate_project, simulate_scene, simulate_ticks, json);
    }

    if (*batch_command) {
        return engine::cli::batch(batch_project, batch_scene, batch_file, batch_undo, json,
                                  dry_run);
    }

    if (*screenshot_command) {
        return engine::cli::screenshot(screenshot_project, screenshot_scene, screenshot_out,
                                       screenshot_frames, screenshot_width, screenshot_height,
                                       json);
    }

    std::cerr << "error: no command\n" << app.help() << '\n';
    return 2;
}
