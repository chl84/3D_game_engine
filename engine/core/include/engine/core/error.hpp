#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <tl/expected.hpp>
#include <utility>

namespace engine::core {

enum class ErrorCode : std::uint8_t {
    Usage,
    Validation,
    NotFound,
    Conflict,
    Io,
    Unsupported,
    Precondition,
    Internal,
    Cancelled,
};

[[nodiscard]] const char* error_code_name(ErrorCode code) noexcept;
[[nodiscard]] int exit_code(ErrorCode code) noexcept;

struct Error {
    ErrorCode code{ErrorCode::Internal};
    std::string message;
    std::string hint;
    nlohmann::json details = nlohmann::json::object();

    [[nodiscard]] nlohmann::json to_json() const;
};

template <typename T> using Result = tl::expected<T, Error>;

[[nodiscard]] Error make_error(ErrorCode code, std::string message, std::string hint = {},
                               nlohmann::json details = nlohmann::json::object());

template <typename T> [[nodiscard]] Result<T> make_failure(Error error) {
    return tl::make_unexpected(std::move(error));
}

} // namespace engine::core
