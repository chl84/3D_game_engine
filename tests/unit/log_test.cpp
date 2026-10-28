#include <catch2/catch_test_macros.hpp>

#include <engine/core/log.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>

#include "temp_dir.hpp"

using engine::core::Log;
using engine::core::LogLevel;
using engine::core::LogOptions;

TEST_CASE("log writes filtered JSON lines", "[core][log]") {
    engine::test::TempDir temp("log");
    const auto path = temp.path() / "engine.jsonl";

    LogOptions options;
    options.level = LogLevel::Debug;
    options.console = false;
    options.json_file = path;

    {
        Log log(options);
        log.info("core", "hello", {{"answer", 42}});
        log.debug("core", "debug message");
        log.trace("core", "hidden");
    }

    std::ifstream input(path);
    REQUIRE(input.is_open());

    std::vector<nlohmann::json> records;
    std::string line;
    while (std::getline(input, line)) {
        records.push_back(nlohmann::json::parse(line));
    }

    REQUIRE(records.size() == 2);
    REQUIRE(records[0].at("level") == "info");
    REQUIRE(records[0].at("category") == "core");
    REQUIRE(records[0].at("message") == "hello");
    REQUIRE(records[0].at("fields").at("answer") == 42);
    REQUIRE(records[0].at("thread").is_string());
    REQUIRE_FALSE(records[0].at("thread").get<std::string>().empty());
    REQUIRE(records[0].at("ts").is_string());
    REQUIRE_FALSE(records[0].at("ts").get<std::string>().empty());
    REQUIRE(records[1].at("level") == "debug");
    REQUIRE(records[1].at("fields").is_object());
}

TEST_CASE("log level names are stable", "[core][log]") {
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Trace)) == "trace");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Debug)) == "debug");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Info)) == "info");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Warn)) == "warn");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Error)) == "error");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Critical)) == "critical");
}
