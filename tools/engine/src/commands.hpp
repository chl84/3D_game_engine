#pragma once

#include <string>

namespace engine::cli {

int project_create(const std::string& root, const std::string& name, bool json);
int project_validate(const std::string& root, bool json);
int project_unlock(const std::string& root, bool force, bool json);

int scene_create(const std::string& project_root, const std::string& name,
                 const std::string& relative_path, bool json);
int scene_inspect(const std::string& project_root, const std::string& scene_path, bool json);
int scene_validate(const std::string& project_root, const std::string& scene_path, bool json);

int entity_create(const std::string& project_root, const std::string& scene_path,
                  const std::string& name, const std::string& parent, const std::string& id,
                  bool json);
int entity_delete(const std::string& project_root, const std::string& scene_path,
                  const std::string& id, bool json);
int entity_reparent(const std::string& project_root, const std::string& scene_path,
                    const std::string& id, const std::string& parent, bool json);

int component_set(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component,
                  const std::string& value, bool json);
int component_add(const std::string& project_root, const std::string& scene_path,
                  const std::string& entity_id, const std::string& component, bool json);
int component_remove(const std::string& project_root, const std::string& scene_path,
                     const std::string& entity_id, const std::string& component, bool json);

int simulate(const std::string& project_root, const std::string& scene_path, int ticks, bool json);
int batch(const std::string& project_root, const std::string& scene_path, const std::string& file,
          int undo_count, bool json);

} // namespace engine::cli
