#include "doctor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace {

[[nodiscard]] nlohmann::json device(std::string name, bool supported) {
    return {
        {"name", std::move(name)},
        {"descriptorIndexing", {{"supported", supported}}},
    };
}

} // namespace

TEST_CASE("doctor accepts a mixed set when one device is supported", "[unit][doctor]") {
    const nlohmann::json devices = nlohmann::json::array({
        device("supported-gpu", true),
        device("unsupported-gpu", false),
    });

    CHECK(engine::cli::any_device_supports_descriptor_indexing(devices));
    CHECK_FALSE(engine::cli::descriptor_indexing_requirement_failed(devices));
}

TEST_CASE("doctor fails only when every enumerated device is unsupported", "[unit][doctor]") {
    const nlohmann::json all_unsupported = nlohmann::json::array({
        device("unsupported-a", false),
        device("unsupported-b", false),
    });
    CHECK_FALSE(engine::cli::any_device_supports_descriptor_indexing(all_unsupported));
    CHECK(engine::cli::descriptor_indexing_requirement_failed(all_unsupported));

    const nlohmann::json empty = nlohmann::json::array();
    CHECK_FALSE(engine::cli::any_device_supports_descriptor_indexing(empty));
    CHECK_FALSE(engine::cli::descriptor_indexing_requirement_failed(empty));
}
