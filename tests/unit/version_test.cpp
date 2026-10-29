#include <catch2/catch_test_macros.hpp>

#include <engine/core/version.hpp>

TEST_CASE("version metadata is populated", "[core][version]") {
    REQUIRE_FALSE(engine::core::version().empty());
    REQUIRE_FALSE(engine::core::git_commit().empty());
    REQUIRE_FALSE(engine::core::build_type().empty());
    REQUIRE_FALSE(engine::core::compiler().empty());
    REQUIRE_FALSE(engine::core::platform().empty());
    REQUIRE(engine::core::cxx_standard() == "20");
}
