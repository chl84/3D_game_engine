#include <engine/render/camera.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cmath>

TEST_CASE("camera projects the test triangle in front of the viewer", "[render][camera]") {
    engine::render::Camera camera;
    const glm::mat4 view_projection = camera.view_projection(16.0F / 9.0F);

    const glm::vec4 center = view_projection * glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
    REQUIRE(center.w > 0.0F);
    CHECK(center.x == Catch::Approx(0.0F).margin(0.0001F));
    CHECK(center.y == Catch::Approx(0.0F).margin(0.0001F));
    const float center_depth = center.z / center.w;
    CHECK(center_depth > 0.0F);
    CHECK(center_depth < 1.0F);

    const glm::vec4 top = view_projection * glm::vec4(0.0F, 0.6F, 0.0F, 1.0F);
    const glm::vec4 bottom = view_projection * glm::vec4(0.0F, -0.5F, 0.0F, 1.0F);
    REQUIRE(top.w > 0.0F);
    REQUIRE(bottom.w > 0.0F);
    CHECK(top.y < 0.0F);
    CHECK(bottom.y > 0.0F);
    CHECK(top.y / top.w > -1.0F);
    CHECK(top.y / top.w < 1.0F);
    CHECK(bottom.y / bottom.w > -1.0F);
    CHECK(bottom.y / bottom.w < 1.0F);
}

TEST_CASE("camera aspect ratio guards against a zero width", "[render][camera]") {
    engine::render::Camera camera;
    const glm::mat4 projection = camera.projection(0.0F);
    CHECK(std::isfinite(projection[0][0]));
    CHECK(projection[0][0] > 0.0F);
}
