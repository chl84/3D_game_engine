#include <catch2/catch_test_macros.hpp>

#include <engine/core/error.hpp>

#include <nlohmann/json.hpp>
#include <string>

using engine::core::Error;
using engine::core::ErrorCode;
using engine::core::make_error;
using engine::core::Result;

TEST_CASE("error codes have stable names and exit codes", "[core][error]") {
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Usage)) == "E_USAGE");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Validation)) == "E_VALIDATION");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::NotFound)) == "E_NOT_FOUND");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Conflict)) == "E_CONFLICT");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Io)) == "E_IO");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Unsupported)) == "E_UNSUPPORTED");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Precondition)) ==
            "E_PRECONDITION");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Internal)) == "E_INTERNAL");
    REQUIRE(std::string(engine::core::error_code_name(ErrorCode::Cancelled)) == "E_CANCELLED");

    REQUIRE(engine::core::exit_code(ErrorCode::Usage) == 2);
    REQUIRE(engine::core::exit_code(ErrorCode::Validation) == 3);
    REQUIRE(engine::core::exit_code(ErrorCode::NotFound) == 4);
    REQUIRE(engine::core::exit_code(ErrorCode::Conflict) == 5);
    REQUIRE(engine::core::exit_code(ErrorCode::Io) == 6);
    REQUIRE(engine::core::exit_code(ErrorCode::Unsupported) == 7);
    REQUIRE(engine::core::exit_code(ErrorCode::Precondition) == 8);
    REQUIRE(engine::core::exit_code(ErrorCode::Internal) == 1);
    REQUIRE(engine::core::exit_code(ErrorCode::Cancelled) == 130);
}

TEST_CASE("errors serialize to JSON", "[core][error]") {
    const Error error = make_error(ErrorCode::NotFound, "Entity not found.", "Check the UUID.",
                                   {{"entity", "abc"}});
    const nlohmann::json json = error.to_json();

    REQUIRE(json.at("code") == "E_NOT_FOUND");
    REQUIRE(json.at("message") == "Entity not found.");
    REQUIRE(json.at("hint") == "Check the UUID.");
    REQUIRE(json.at("details").at("entity") == "abc");
}

TEST_CASE("results carry either a value or an error", "[core][error]") {
    Result<int> value = 42;
    REQUIRE(value.has_value());
    REQUIRE(*value == 42);

    auto failure =
        engine::core::make_failure<int>(make_error(ErrorCode::Validation, "Invalid value."));
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(failure.error().code == ErrorCode::Validation);
}
