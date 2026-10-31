#pragma once

#include <engine/command/command.hpp>
#include <engine/scene/project.hpp>
#include <string>
#include <vector>

namespace engine::cli {
// CLI and live mode share this exact handler registration and CommandBus.
void register_project_commands(command::CommandBus& bus, scene::Project& project,
                               scene::Scene& scene, const core::ReflectionRegistry& registry,
                               bool project_sources_only = false);
[[nodiscard]] nlohmann::json project_command_schemas();
[[nodiscard]] core::Result<command::CommandOutput>
capture_scene(const scene::Project& project, const scene::Scene& scene, const std::string& out_path,
              int frames, int width, int height, bool inline_png,
              std::string_view scene_label = "live");
int live_server(const std::string& root, const std::string& scene_path, int timeout_ms,
                bool as_json);
int live_call(const std::string& root, const std::string& method, const std::string& params_text,
              bool as_json);

int project_create(const std::string& root, const std::string& name, bool json, bool dry_run);
int project_validate(const std::string& root, bool json);
int project_unlock(const std::string& root, bool force, bool json, bool dry_run);

int scene_create(const std::string& project_root, const std::string& name,
                 const std::string& relative_path, bool json, bool dry_run);
int scene_inspect(const std::string& project_root, const std::string& scene_path, bool json);
int scene_validate(const std::string& project_root, const std::string& scene_path, bool json);

int entity_create(const std::string& project_root, const std::string& scene_path,
                  const std::string& name, const std::string& parent, const std::string& id,
                  bool json, bool dry_run);
int entity_delete(const std::string& project_root, const std::string& scene_path,
                  const std::string& id, bool json, bool dry_run);
int entity_reparent(const std::string& project_root, const std::string& scene_path,
                    const std::string& id, const std::string& parent, bool json, bool dry_run);

int component_set(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component,
                  const std::string& value, bool json, bool dry_run);
int component_add(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component, bool json,
                  bool dry_run);
int component_remove(const std::string& project_root, const std::string& scene_path,
                     const std::string& entity_id, const std::string& component, bool json,
                     bool dry_run);

int simulate(const std::string& project_root, const std::string& scene_path, int ticks, bool json);
int screenshot(const std::string& project_root, const std::string& scene_path,
               const std::string& out_path, int frames, int width, int height, bool json);
int run_window(const std::string& project_root, const std::string& scene_path, int frames,
               int width, int height, bool hidden, bool json);
int asset_execute(const std::string& operation, const std::string& project_root,
                  const std::string& source, const std::string& asset_id, const std::string& name,
                  bool json, bool dry_run);
int asset_place(const std::string& project_root, const std::string& scene_path,
                const std::string& asset_id, const std::string& name,
                const std::vector<double>& position, bool json, bool dry_run);
int schema(bool json);
int batch(const std::string& project_root, const std::string& scene_path, const std::string& file,
          int undo_count, bool json, bool dry_run);

} // namespace engine::cli
