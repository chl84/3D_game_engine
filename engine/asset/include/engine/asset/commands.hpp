#pragma once

#include <engine/command/command.hpp>
#include <filesystem>

namespace engine::scene {
class Scene;
}

namespace engine::asset {

[[nodiscard]] nlohmann::json asset_command_schemas();

// The caller holds the project lock for import/reimport and supplies the
// project root. Preview executes the same validation/cooking without writes.
// File import/reimport are not undoable and must not be run in a transaction.
[[nodiscard]] core::Result<void> register_asset_commands(command::CommandBus& bus,
                                                         const std::filesystem::path& project_root,
                                                         bool dry_run = false);
[[nodiscard]] core::Result<void> register_asset_commands(command::CommandBus& bus,
                                                         const std::filesystem::path& project_root,
                                                         bool dry_run, bool project_sources_only);

// Registers undoable asset.place@1 against a caller-owned scene. Placement
// validates the cooked asset, then stores only its stable UUID and transform.
// The caller holds the project lock and is responsible for saving the scene.
[[nodiscard]] core::Result<void>
register_asset_scene_commands(command::CommandBus& bus, const std::filesystem::path& project_root,
                              scene::Scene& scene);

} // namespace engine::asset
