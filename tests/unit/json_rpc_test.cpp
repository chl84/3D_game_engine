#include <catch2/catch_test_macros.hpp>
#include <engine/control/json_rpc.hpp>
#include <string>

TEST_CASE("live RPC authenticates before exposing request details", "[live][rpc]") {
    using namespace engine::control;
    const std::string credential(64, 't');
    nlohmann::json valid = {
        {"jsonrpc", "2.0"},     {"id", 7},
        {"method", "ping"},     {"params", nlohmann::json::object()},
        {"protocolVersion", 1}, {"token", credential},
    };
    auto parsed = parse_rpc(valid.dump(), credential);
    REQUIRE(parsed.request.has_value());
    CHECK(parsed.request.value_or(RpcRequest{}).method == "ping");
    CHECK_FALSE(parsed.request.value_or(RpcRequest{}).notification);
    valid.erase("id");
    REQUIRE(parse_rpc(valid.dump(), credential).request.has_value());
    CHECK(parse_rpc(valid.dump(), credential).request.value_or(RpcRequest{}).notification);
    const auto denied = rpc_error(nullptr, -32001, "Unauthorized");
    for (const auto& value : {
             nlohmann::json(nullptr),
             nlohmann::json(17),
             nlohmann::json("wrong"),
             nlohmann::json(std::string(64, 'x')),
         }) {
        valid["token"] = value;
        valid["method"] = {{"malicious", credential}};
        valid["id"] = credential;
        CHECK(parse_rpc(valid.dump(), credential).error == denied);
        CHECK(parse_rpc(valid.dump(), credential).error.dump().find(credential) ==
              std::string::npos);
    }
}
TEST_CASE("live RPC parser rejects hostile framing, depth and duplicate members", "[live][rpc]") {
    using namespace engine::control;
    const std::string credential(64, 't');
    for (const auto& wire : {
             std::string("{"),
             std::string(70, '[') + std::string(70, ']'),
             std::string(kMaxRequestBytes + 1, ' '),
             std::string(R"({"token":"a","token":"b"})"),
             std::string("{}{}"),
             std::string("\xff"),
         }) {
        auto parsed = parse_rpc(wire, credential);
        CHECK_FALSE(parsed.request.has_value());
        CHECK(parsed.error.at("error").at("code") == -32700);
    }
    nlohmann::json request = {
        {"jsonrpc", "2.0"},    {"id", 1}, {"method", "ping"}, {"protocolVersion", 1},
        {"token", credential},
    };
    for (const auto& patch : {
             nlohmann::json{{"id", true}},
             nlohmann::json{{"params", nlohmann::json::array()}},
             nlohmann::json{{"protocolVersion", 2}},
             nlohmann::json{{"jsonrpc", "1.0"}},
             nlohmann::json{{"extra", 1}},
         }) {
        auto invalid = request;
        invalid.update(patch);
        CHECK_FALSE(parse_rpc(invalid.dump(), credential).request.has_value());
    }
    CHECK(base64("") == "");
    CHECK(base64("f") == "Zg==");
    CHECK(base64("fo") == "Zm8=");
    CHECK(base64("foo") == "Zm9v");
}
