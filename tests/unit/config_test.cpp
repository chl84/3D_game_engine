#include <catch2/catch_test_macros.hpp>

#include <engine/core/config.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

#include "temp_dir.hpp"

using engine::core::Config;
using engine::core::ErrorCode;

TEST_CASE("config reads nested values", "[core][config]") {
    const auto config = Config::from_json(nlohmann::json::parse(R"({
        "server": {"port": 8080, "name": "local"},
        "name": "engine",
        "features": ["a", "b"]
    })"));
    REQUIRE(config.has_value());

    const auto port = config->get<int>("/server/port");
    REQUIRE(port.has_value());
    REQUIRE(*port == 8080);

    const auto name = config->get<std::string>("/name");
    REQUIRE(name.has_value());
    REQUIRE(*name == "engine");

    REQUIRE(config->contains("/features"));
    REQUIRE_FALSE(config->contains("/missing"));
    REQUIRE(config->get_or<int>("/server/missing", 42) == 42);
}

TEST_CASE("config rejects invalid roots and types", "[core][config]") {
    REQUIRE_FALSE(Config::from_json(nlohmann::json::array()));

    const auto config = Config::from_json(nlohmann::json::parse(R"({"answer": "forty-two"})"));
    REQUIRE(config.has_value());

    const auto answer = config->get<int>("/answer");
    REQUIRE_FALSE(answer.has_value());
    REQUIRE(answer.error().code == ErrorCode::Validation);

    const auto missing = config->get<int>("/missing");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == ErrorCode::NotFound);

    REQUIRE_FALSE(config->contains("not a pointer"));
}

TEST_CASE("config loads from a file", "[core][config]") {
    engine::test::TempDir temp("config");
    const auto path = temp.path() / "config.json";

    {
        std::ofstream output(path);
        output << R"({"window": {"width": 1920, "height": 1080}})";
    }

    const auto config = Config::load(path);
    REQUIRE(config.has_value());

    const auto width = config->get<int>("/window/width");
    REQUIRE(width.has_value());
    REQUIRE(*width == 1920);
}

TEST_CASE("config load reports missing and invalid files", "[core][config]") {
    engine::test::TempDir temp("config_errors");

    const auto missing = Config::load(temp.path() / "missing.json");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == ErrorCode::Io);

    const auto invalid_path = temp.path() / "invalid.json";
    {
        std::ofstream output(invalid_path);
        output << "{not json";
    }

    const auto invalid = Config::load(invalid_path);
    REQUIRE_FALSE(invalid.has_value());
    REQUIRE(invalid.error().code == ErrorCode::Validation);
}

TEST_CASE("config merge is recursive and overlay wins", "[core][config]") {
    const auto base = Config::from_json(nlohmann::json::parse(R"({
        "a": {"b": 1, "c": 2},
        "list": [1, 2],
        "keep": true
    })"));
    const auto overlay = Config::from_json(nlohmann::json::parse(R"({
        "a": {"b": 3},
        "list": [3],
        "new": "value"
    })"));
    REQUIRE(base.has_value());
    REQUIRE(overlay.has_value());

    Config merged = *base;
    merged.merge(*overlay);

    REQUIRE(merged.get_or<int>("/a/b", 0) == 3);
    REQUIRE(merged.get_or<int>("/a/c", 0) == 2);
    REQUIRE(merged.get_or<bool>("/keep", false));
    REQUIRE(merged.get_or<std::string>("/new", "") == "value");
    REQUIRE(merged.get<std::string>("/list").has_value() == false);
    REQUIRE(merged.get<nlohmann::json>("/list")->size() == 1);
}
