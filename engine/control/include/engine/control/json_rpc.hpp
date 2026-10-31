#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace engine::control {
inline constexpr std::size_t kMaxRequestBytes = std::size_t{64} * 1024U;
inline constexpr std::size_t kMaxReplyBytes = std::size_t{4} * 1024U * 1024U;
inline constexpr int kMaxJsonDepth = 32;
struct RpcRequest {
    nlohmann::json id;
    std::string method;
    nlohmann::json params;
    bool notification{false};
};
struct ParsedRpc {
    std::optional<RpcRequest> request;
    nlohmann::json error;
};
[[nodiscard]] nlohmann::json rpc_error(const nlohmann::json& id, int code,
                                       std::string_view message);
[[nodiscard]] nlohmann::json rpc_result(const nlohmann::json& id, nlohmann::json result);
// Bounded parser with strict UTF-8, depth/duplicate-key rejection, and auth
// before protocol/method details. Never echoes credentials or parser exceptions.
[[nodiscard]] ParsedRpc parse_rpc(std::string_view input, std::string_view expected_token);
[[nodiscard]] std::string base64(std::string_view bytes);
} // namespace engine::control
