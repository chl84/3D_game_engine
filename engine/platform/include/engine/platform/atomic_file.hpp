#pragma once

#include <engine/core/error.hpp>

#include <filesystem>
#include <string_view>

namespace engine::platform {

[[nodiscard]] core::Result<void> atomic_write_file(const std::filesystem::path& path,
                                                   std::string_view content);

} // namespace engine::platform
