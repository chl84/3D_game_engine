#pragma once

#include <engine/command/command.hpp>
#include <filesystem>

namespace engine::asset {

[[nodiscard]] nlohmann::json asset_command_schemas();

// The caller holds the project lock for import/reimport and supplies the
// project root. Preview executes the same validation/cooking without writes.
// File import/reimport are not undoable and must not be run in a transaction.
[[nodiscard]] core::Result<void> register_asset_commands(command::CommandBus& bus,
                                                         const std::filesystem::path& project_root,
                                                         bool dry_run = false);

} // namespace engine::asset
