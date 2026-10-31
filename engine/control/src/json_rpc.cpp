#include <array>
#include <cstdint>
#include <engine/control/json_rpc.hpp>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::control {
using nlohmann::json;
json rpc_error(const json& id, int code, std::string_view message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}
json rpc_result(const json& id, json result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}
namespace {
bool token_matches(std::string_view a, std::string_view b) {
    if (a.size() != 64 || b.size() != a.size()) {
        return false;
    }
    unsigned difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        difference |= static_cast<unsigned>(static_cast<unsigned char>(a[i]) ^
                                            static_cast<unsigned char>(b[i]));
    }
    return difference == 0;
}
bool depth_ok(std::string_view input) {
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (const char c : input) {
        if (quoted) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                quoted = false;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == '{' || c == '[') {
            if (++depth > kMaxJsonDepth) {
                return false;
            }
        } else if ((c == '}' || c == ']') && (--depth < 0)) {
            return false;
        }
    }
    return depth == 0 && !quoted;
}
} // namespace
ParsedRpc parse_rpc(std::string_view input, std::string_view expected_token) {
    auto reject = [](int code, std::string_view message) -> ParsedRpc {
        return {std::nullopt, rpc_error(nullptr, code, message)};
    };
    if (input.size() > kMaxRequestBytes || !depth_ok(input)) {
        return reject(-32700, "Parse error");
    }
    json document;
    try {
        std::vector<std::set<std::string>> keys;
        const auto callback = [&keys](int, json::parse_event_t event, json& parsed) {
            if (event == json::parse_event_t::object_start) {
                keys.emplace_back();
            } else if (event == json::parse_event_t::key) {
                if (!keys.back().insert(parsed.get<std::string>()).second) {
                    throw std::runtime_error("duplicate key");
                }
            } else if (event == json::parse_event_t::object_end) {
                keys.pop_back();
            }
            return true;
        };
        document = json::parse(input, callback, false);
    } catch (const std::exception&) {
        return reject(-32700, "Parse error");
    }
    if (document.is_discarded()) {
        return reject(-32700, "Parse error");
    }
    if (!document.is_object() || !document.contains("token") || !document["token"].is_string() ||
        !token_matches(document["token"].get_ref<const std::string&>(), expected_token)) {
        return reject(-32001, "Unauthorized");
    }
    for (const auto& [key, value] : document.items()) {
        (void)value;
        if (key != "jsonrpc" && key != "id" && key != "method" && key != "params" &&
            key != "token" && key != "protocolVersion") {
            return reject(-32600, "Invalid Request");
        }
    }
    if (document.value("jsonrpc", json()) != json("2.0") ||
        document.value("protocolVersion", json()) != json(1) || !document.contains("method") ||
        !document["method"].is_string() ||
        (document.contains("params") && !document["params"].is_object())) {
        return reject(-32600, "Invalid Request");
    }
    const auto id = document.value("id", json());
    if (!(id.is_null() || id.is_string() || id.is_number_integer()) ||
        (id.is_string() && id.get_ref<const std::string&>().size() > 128)) {
        return reject(-32600, "Invalid Request");
    }
    return {
        RpcRequest{
            id,
            document["method"].get<std::string>(),
            document.value("params", json::object()),
            !document.contains("id"),
        },
        {},
    };
}
std::string base64(std::string_view bytes) {
    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        std::uint32_t word = static_cast<unsigned char>(bytes[i]);
        word =
            (word << 8U) | (i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0U);
        word =
            (word << 8U) | (i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0U);
        result.push_back(alphabet[(word >> 18U) & 63U]);
        result.push_back(alphabet[(word >> 12U) & 63U]);
        result.push_back(i + 1 < bytes.size() ? alphabet[(word >> 6U) & 63U] : '=');
        result.push_back(i + 2 < bytes.size() ? alphabet[word & 63U] : '=');
    }
    return result;
}
} // namespace engine::control
