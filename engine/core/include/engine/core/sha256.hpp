#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace engine::core {

// Lower-case hexadecimal SHA-256 digest.
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);
[[nodiscard]] std::string sha256_hex(std::string_view text);

} // namespace engine::core
