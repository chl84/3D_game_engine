#pragma once

#include <string_view>

namespace engine::core {

[[nodiscard]] std::string_view version() noexcept;
[[nodiscard]] std::string_view git_commit() noexcept;
[[nodiscard]] std::string_view build_type() noexcept;
[[nodiscard]] std::string_view compiler() noexcept;
[[nodiscard]] std::string_view platform() noexcept;
[[nodiscard]] std::string_view cxx_standard() noexcept;
[[nodiscard]] std::string_view os_name() noexcept;
[[nodiscard]] std::string_view os_version() noexcept;

} // namespace engine::core
