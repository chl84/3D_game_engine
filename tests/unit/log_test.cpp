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

TEST_CASE("timestamps cover calendar boundaries", "[core][log]") {
    using namespace std::chrono;

    const auto make_time = [](int year, unsigned month, unsigned day, int hour, int minute,
                              int second, int millisecond) {
        const auto day_point =
            sys_days{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}};
        return system_clock::time_point{day_point + hours{hour} + minutes{minute} +
                                        seconds{second} + milliseconds{millisecond}};
    };

    REQUIRE(engine::core::format_timestamp_utc(make_time(1970, 1, 1, 0, 0, 0, 0)) ==
            "1970-01-01T00:00:00.000Z");
    REQUIRE(engine::core::format_timestamp_utc(make_time(1970, 1, 1, 0, 0, 0, 1)) ==
            "1970-01-01T00:00:00.001Z");
    REQUIRE(engine::core::format_timestamp_utc(make_time(2024, 2, 29, 12, 34, 56, 789)) ==
            "2024-02-29T12:34:56.789Z");
    REQUIRE(engine::core::format_timestamp_utc(make_time(2024, 3, 1, 0, 0, 0, 0)) ==
            "2024-03-01T00:00:00.000Z");
    REQUIRE(engine::core::format_timestamp_utc(make_time(2023, 12, 31, 23, 59, 59, 999)) ==
            "2023-12-31T23:59:59.999Z");
    REQUIRE(engine::core::format_timestamp_utc(make_time(2024, 1, 1, 0, 0, 0, 0)) ==
            "2024-01-01T00:00:00.000Z");
}

TEST_CASE("log level names are stable", "[core][log]") {
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Trace)) == "trace");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Debug)) == "debug");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Info)) == "info");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Warn)) == "warn");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Error)) == "error");
    REQUIRE(std::string(engine::core::log_level_name(LogLevel::Critical)) == "critical");
}
