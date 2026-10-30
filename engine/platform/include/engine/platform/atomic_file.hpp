#pragma once

#include <engine/core/error.hpp>

#include <filesystem>
#include <string_view>

namespace engine::platform {

[[nodiscard]] core::Result<void> atomic_write_file(const std::filesystem::path& path,
                                                   std::string_view content);

// Private from creation, including its temporary file: POSIX0600 or a protected
// Windows DACL containing only the current user. Fails closed on setup errors.
[[nodiscard]] core::Result<void> atomic_write_private_file(const std::filesystem::path& path,
                                                           std::string_view content);

} // namespace engine::platform
