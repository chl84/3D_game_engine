#include <catch2/catch_test_macros.hpp>

#include <engine/core/uuid.hpp>

#include <string>

using engine::core::Uuid;
using engine::core::UuidGenerator;

TEST_CASE("UUID parse and format round-trip", "[core][uuid]") {
    const auto parsed = Uuid::parse("550e8400-e29b-41d4-a716-446655440000");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->to_string() == "550e8400-e29b-41d4-a716-446655440000");
    REQUIRE_FALSE(parsed->is_nil());

    const auto uppercase = Uuid::parse("550E8400-E29B-41D4-A716-446655440000");
    REQUIRE(uppercase.has_value());
    REQUIRE(uppercase->to_string() == "550e8400-e29b-41d4-a716-446655440000");
    REQUIRE(*uppercase == *parsed);
}

TEST_CASE("UUID parse rejects invalid input", "[core][uuid]") {
    REQUIRE_FALSE(Uuid::parse(""));
    REQUIRE_FALSE(Uuid::parse("550e8400e29b41d4a716446655440000"));
    REQUIRE_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-44665544000"));
    REQUIRE_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-4466554400000"));
    REQUIRE_FALSE(Uuid::parse("550e8400-e29b-41d4-a716-44665544000g"));
    REQUIRE_FALSE(Uuid::parse("550e8400xe29b-41d4-a716-446655440000"));
}

TEST_CASE("generated UUIDs are version 4", "[core][uuid]") {
    const auto uuid = Uuid::generate();
    REQUIRE_FALSE(uuid.is_nil());
    REQUIRE((uuid.bytes()[6] & 0xF0U) == 0x40U);
    REQUIRE((uuid.bytes()[8] & 0xC0U) == 0x80U);
    REQUIRE(uuid.to_string().size() == 36);
}

TEST_CASE("seeded UUID generator is deterministic", "[core][uuid]") {
    UuidGenerator first(1234, 7);
    UuidGenerator second(1234, 7);

    for (int index = 0; index < 8; ++index) {
        REQUIRE(first.next() == second.next());
    }

    UuidGenerator different_seed(1235, 7);
    UuidGenerator different_stream(1234, 8);
    UuidGenerator reference(1234, 7);

    REQUIRE(reference.next() != different_seed.next());
    REQUIRE(reference.next() != different_stream.next());
}

TEST_CASE("UUID hashing is stable", "[core][uuid]") {
    const auto uuid = Uuid::parse("550e8400-e29b-41d4-a716-446655440000");
    REQUIRE(uuid.has_value());
    REQUIRE(uuid->hash() == uuid->hash());
}
