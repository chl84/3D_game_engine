#include <engine/core/error.hpp>

#include <utility>

namespace engine::core {

const char* error_code_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Usage:
        return "E_USAGE";
    case ErrorCode::Validation:
        return "E_VALIDATION";
    case ErrorCode::NotFound:
        return "E_NOT_FOUND";
    case ErrorCode::Conflict:
        return "E_CONFLICT";
    case ErrorCode::Io:
        return "E_IO";
    case ErrorCode::Unsupported:
        return "E_UNSUPPORTED";
    case ErrorCode::Precondition:
        return "E_PRECONDITION";
    case ErrorCode::Internal:
        return "E_INTERNAL";
    case ErrorCode::Cancelled:
        return "E_CANCELLED";
    }
    return "E_INTERNAL";
}

int exit_code(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Usage:
        return 2;
    case ErrorCode::Validation:
        return 3;
    case ErrorCode::NotFound:
        return 4;
    case ErrorCode::Conflict:
        return 5;
    case ErrorCode::Io:
        return 6;
    case ErrorCode::Unsupported:
        return 7;
    case ErrorCode::Precondition:
        return 8;
    case ErrorCode::Internal:
        return 1;
    case ErrorCode::Cancelled:
        return 130;
    }
    return 1;
}

nlohmann::json Error::to_json() const {
    nlohmann::json value{
        {"code", error_code_name(code)},
        {"message", message},
        {"details", details},
    };
    if (!hint.empty()) {
        value["hint"] = hint;
    }
    return value;
}

Error make_error(ErrorCode code, std::string message, std::string hint, nlohmann::json details) {
    return Error{code, std::move(message), std::move(hint), std::move(details)};
}

} // namespace engine::core
