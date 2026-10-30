#include <engine/asset/image_decode.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

using engine::asset::decode_image;
using engine::asset::ImageMimeType;
using engine::core::ErrorCode;

[[nodiscard]] std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    const std::string content{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(content.size());
    for (std::size_t index = 0; index < content.size(); ++index) {
        bytes[index] = static_cast<std::byte>(content[index]);
    }
    return bytes;
}

[[nodiscard]] std::span<const std::byte> as_span(const std::vector<std::byte>& bytes) {
    return std::span<const std::byte>(bytes.data(), bytes.size());
}

[[nodiscard]] std::filesystem::path fixture(const char* name) {
    return std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / name;
}

} // namespace

TEST_CASE("PNG and JPEG decode to RGBA8", "[asset][image]") {
    const auto png = read_bytes(fixture("m3_basecolor.png"));
    auto decoded = decode_image(as_span(png), ImageMimeType::Png);
    REQUIRE(decoded.has_value());
    CHECK(decoded->width == 4U);
    CHECK(decoded->height == 4U);
    CHECK(decoded->rgba8.size() == 4U * 4U * 4U);
    CHECK(decoded->rgba8[3] == 255U);

    const auto jpeg = read_bytes(fixture("texture.jpg"));
    auto jpeg_decoded = decode_image(as_span(jpeg), ImageMimeType::Jpeg);
    REQUIRE(jpeg_decoded.has_value());
    CHECK(jpeg_decoded->width == 2U);
    CHECK(jpeg_decoded->height == 2U);
    CHECK(jpeg_decoded->rgba8.size() == 2U * 2U * 4U);
}

TEST_CASE("invalid and oversized images fail before allocation", "[asset][image]") {
    const auto png = read_bytes(fixture("m3_basecolor.png"));
    SECTION("truncated") {
        auto truncated = png;
        truncated.resize(png.size() / 2U);
        const auto result = decode_image(as_span(truncated), ImageMimeType::Png);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
    }

    SECTION("corrupt") {
        const std::vector<std::byte> garbage(64U, std::byte{0x7F});
        const auto result = decode_image(as_span(garbage), ImageMimeType::Png);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
    }

    SECTION("oversized") {
        const auto oversized = read_bytes(fixture("oversized.png"));
        const auto result = decode_image(as_span(oversized), ImageMimeType::Png);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["maxDimension"] == engine::asset::kMaxTextureDimension);
    }
}
