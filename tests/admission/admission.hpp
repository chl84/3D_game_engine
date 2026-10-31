#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace admission {

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] nlohmann::json verify_lua();
[[nodiscard]] nlohmann::json verify_jolt();
[[nodiscard]] nlohmann::json benchmark_lua(std::string_view mode, std::size_t count);

} // namespace admission
