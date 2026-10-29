#include <engine/command/command.hpp>

#include <catch2/catch_test_macros.hpp>

using engine::command::CommandBus;
using engine::command::CommandLog;
using engine::command::CommandOutput;
using engine::command::CommandRequest;
using engine::command::CommandResult;
using engine::core::ErrorCode;

namespace {

[[nodiscard]] nlohmann::json echo_schema() {
    return {
        {"$schema", "http://json-schema.org/draft-07/schema#"}, {"type", "object"},
        {"required", nlohmann::json::array({"value"})},         {"additionalProperties", false},
        {"properties", {{"value", {{"type", "integer"}}}}},
    };
}

void add_echo(CommandBus& bus) {
    const auto added = bus.add("test.echo@1", echo_schema(), [](const nlohmann::json& arguments) {
        return engine::core::Result<CommandOutput>(CommandOutput{arguments, {"echoed"}});
    });
    REQUIRE(added.has_value());
}

} // namespace

TEST_CASE("command bus validates and executes a command", "[command]") {
    CommandLog log;
    CommandBus bus(log);
    add_echo(bus);

    const CommandResult result = bus.execute(CommandRequest{"test.echo@1", {{"value", 42}}});
    REQUIRE(result.ok);
    REQUIRE(result.data.at("value") == 42);
    REQUIRE(result.warnings == std::vector<std::string>{"echoed"});
    REQUIRE_FALSE(result.error.has_value());

    REQUIRE(log.entries().size() == 1);
    REQUIRE(log.entries().front().sequence == 1);
    REQUIRE(log.entries().front().name == "test.echo@1");
    REQUIRE(log.entries().front().arguments.at("value") == 42);
    REQUIRE(log.entries().front().result.at("status") == "ok");
}

TEST_CASE("command bus rejects invalid arguments and logs the attempt", "[command]") {
    CommandLog log;
    CommandBus bus(log);
    add_echo(bus);

    const CommandResult result = bus.execute(CommandRequest{"test.echo@1", {{"value", "text"}}});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error.value_or(engine::core::Error{}).code == ErrorCode::Validation);
    REQUIRE(log.entries().size() == 1);
    REQUIRE(log.entries().front().result.at("status") == "error");
}

TEST_CASE("command bus rejects unknown commands", "[command]") {
    CommandLog log;
    CommandBus bus(log);

    const CommandResult result = bus.execute(CommandRequest{"test.missing@1", {}});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error.value_or(engine::core::Error{}).code == ErrorCode::NotFound);
}

TEST_CASE("command bus rejects duplicate and malformed registrations", "[command]") {
    CommandLog log;
    CommandBus bus(log);
    add_echo(bus);

    const auto duplicate = bus.add("test.echo@1", echo_schema(), [](const auto&) {
        return engine::core::Result<CommandOutput>(CommandOutput{});
    });
    REQUIRE_FALSE(duplicate.has_value());
    REQUIRE(duplicate.error().code == ErrorCode::Conflict);

    const auto malformed = bus.add("Echo", echo_schema(), [](const auto&) {
        return engine::core::Result<CommandOutput>(CommandOutput{});
    });
    REQUIRE_FALSE(malformed.has_value());
    REQUIRE(malformed.error().code == ErrorCode::Validation);
}

TEST_CASE("command bus converts handler failures into results", "[command]") {
    CommandLog log;
    CommandBus bus(log);
    const auto added = bus.add("test.fail@1", {{"type", "object"}}, [](const auto&) {
        return engine::core::make_failure<CommandOutput>(
            engine::core::make_error(ErrorCode::Io, "handler failed"));
    });
    REQUIRE(added.has_value());

    const CommandResult result =
        bus.execute(CommandRequest{"test.fail@1", nlohmann::json::object()});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error.value_or(engine::core::Error{}).code == ErrorCode::Io);
    REQUIRE(log.entries().size() == 1);
}

TEST_CASE("command registry exposes sorted names and schemas", "[command]") {
    CommandLog log;
    CommandBus bus(log);
    add_echo(bus);

    const auto names = bus.registry().names();
    REQUIRE(names == std::vector<std::string>{"test.echo@1"});
    REQUIRE(bus.registry().schema("test.echo@1") != nullptr);
    REQUIRE(bus.registry().schema("test.missing@1") == nullptr);

    const auto schemas = bus.registry().schemas();
    REQUIRE(schemas.contains("test.echo@1"));
    REQUIRE(schemas.at("test.echo@1").at("type") == "object");
}
