#include <engine/asset/gltf_import.hpp>
#include <engine/asset/tangents.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>

#include <nlohmann/json.hpp>

namespace {

engine::asset::ImportedAsset mirrored_mesh() {
    using namespace engine::asset;
    ImportedAsset asset;
    Material material;
    material.normal_texture = TextureReference{};
    asset.materials.push_back(material);
    Primitive p;
    p.material = 0;
    Vertex a;
    Vertex b;
    Vertex c;
    Vertex d;
    a.position = {0, 0, 0};
    a.uv0 = {0, 0};
    a.tangent = {};
    b.position = {1, 0, 0};
    b.uv0 = {1, 0};
    b.tangent = {};
    c.position = {0, 1, 0};
    c.uv0 = {0, 1};
    c.tangent = {};
    d.position = {1, 1, 0};
    d.uv0 = {0, 0};
    d.tangent = {};
    p.vertices = {a, b, c, d};
    p.indices = {0, 1, 2, 1, 3, 2};
    Mesh mesh;
    mesh.primitives.push_back(p);
    asset.meshes.push_back(mesh);
    return asset;
}

engine::asset::ImportedAsset dense_asset(std::size_t copies, std::size_t corners) {
    auto asset = mirrored_mesh();
    auto primitive = asset.meshes[0].primitives[0];
    primitive.vertices.resize(8);
    for (std::size_t i = 0; i < primitive.vertices.size(); ++i) {
        auto& v = primitive.vertices[i];
        v.position = {1, 1, 1};
        v.normal = {0, 0, 1};
        v.tangent = {};
        v.uv0 = {static_cast<float>(i) / 8.0F, 0};
    }
    primitive.indices.clear();
    for (std::size_t i = 0; i < corners; ++i) {
        primitive.indices.push_back(static_cast<std::uint32_t>(i % 8U));
    }
    asset.meshes[0].primitives.assign(copies, primitive);
    return asset;
}

engine::asset::Primitive dense_group(std::size_t count) {
    auto asset = dense_asset(1, ((count + 2U) / 3U) * 3U);
    auto primitive = asset.meshes[0].primitives[0];
    std::ranges::fill(primitive.indices, 0U);
    primitive.vertices[1].position = {2, 2, 2};
    for (std::size_t i = count; i < primitive.indices.size(); ++i) {
        primitive.indices[i] = 1;
    }
    return primitive;
}

template <std::size_t N>
std::array<std::uint32_t, N> bit_values(const std::array<float, N>& values) {
    std::array<std::uint32_t, N> bits{};
    for (std::size_t i = 0; i < N; ++i) {
        bits[i] = std::bit_cast<std::uint32_t>(values[i]);
    }
    return bits;
}

nlohmann::json geometry(const engine::asset::ImportedAsset& asset) {
    auto meshes = nlohmann::json::array();
    for (const auto& mesh : asset.meshes) {
        auto primitives = nlohmann::json::array();
        for (const auto& p : mesh.primitives) {
            auto vertices = nlohmann::json::array();
            for (const auto& v : p.vertices) {
                // Bit snapshots preserve signed zero and NaN identity too.
                vertices.push_back({
                    bit_values(v.position),
                    bit_values(v.normal),
                    bit_values(v.tangent),
                    bit_values(v.uv0),
                });
            }
            primitives.push_back({{"vertices", vertices}, {"indices", p.indices}});
        }
        meshes.push_back(primitives);
    }
    return meshes;
}

void require_budget_error(const engine::core::Result<void>& result) {
    REQUIRE_FALSE(result);
    CHECK(result.error().code == engine::core::ErrorCode::Validation);
    REQUIRE(result.error().details.contains("estimatedWork"));
    CHECK(result.error().details.at("estimatedWork").get<std::uint64_t>() >
          engine::asset::kMaxTangentWeldPairs);
    CHECK(result.error().details.at("limit").get<std::uint64_t>() ==
          engine::asset::kMaxTangentWeldPairs);
}

} // namespace

TEST_CASE("MikkTSpace splits mirrored tangent spaces instead of averaging them",
          "[asset][tangents]") {
    auto asset = mirrored_mesh();
    REQUIRE(engine::asset::generate_missing_tangents(asset));
    const auto& p = asset.meshes[0].primitives[0];
    REQUIRE(p.indices.size() == 6U);
    CHECK(p.vertices.size() == 6U);
    const auto sign0 = p.vertices[p.indices[0]].tangent[3];
    const auto sign1 = p.vertices[p.indices[3]].tangent[3];
    CHECK(sign0 == -sign1);
    for (const auto& v : p.vertices) {
        CHECK(std::abs((v.tangent[0] * v.tangent[0]) + (v.tangent[1] * v.tangent[1]) +
                       (v.tangent[2] * v.tangent[2]) - 1.0F) < 0.0001F);
    }
    auto second = asset;
    REQUIRE(engine::asset::generate_missing_tangents(second));
    CHECK(second.meshes[0].primitives[0].vertices[0].tangent == p.vertices[0].tangent);
}

TEST_CASE("MikkTSpace matches Blender exported tangents for a baked normal map",
          "[asset][tangents]") {
    const auto fixtures = std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR);
    auto generated = engine::asset::import_glb_file(fixtures / "blender_normal_no_tangents.glb");
    auto reference = engine::asset::import_glb_file(fixtures / "blender_normal_tangents.glb");
    REQUIRE(generated);
    REQUIRE(reference);
    REQUIRE(generated->materials[0].normal_texture.has_value());
    REQUIRE(engine::asset::generate_missing_tangents(*generated));
    const auto& actual = generated->meshes[0].primitives[0];
    const auto& expected = reference->meshes[0].primitives[0];
    REQUIRE(actual.indices.size() == expected.indices.size());
    for (std::size_t i = 0; i < actual.indices.size(); ++i) {
        const auto& a = actual.vertices[actual.indices[i]];
        const auto& b = expected.vertices[expected.indices[i]];
        CHECK(a.position == b.position);
        for (std::size_t axis = 0; axis < 4; ++axis) {
            CHECK(std::abs(a.tangent[axis] - b.tangent[axis]) < 0.0002F);
        }
    }
    const auto original = expected.vertices;
    REQUIRE(engine::asset::generate_missing_tangents(*reference));
    for (std::size_t i = 0; i < original.size(); ++i) {
        CHECK(reference->meshes[0].primitives[0].vertices[i].tangent == original[i].tangent);
    }
}

TEST_CASE("MikkTSpace rejects malformed indices and handles degenerate UVs", "[asset][tangents]") {
    auto asset = mirrored_mesh();
    asset.meshes[0].primitives[0].indices[0] = 999;
    auto invalid = engine::asset::generate_missing_tangents(asset);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == engine::core::ErrorCode::Validation);
    asset = mirrored_mesh();
    for (auto& v : asset.meshes[0].primitives[0].vertices) {
        v.uv0 = {0, 0};
    }
    REQUIRE(engine::asset::generate_missing_tangents(asset));
    for (const auto& v : asset.meshes[0].primitives[0].vertices) {
        for (float f : v.tangent) {
            CHECK(std::isfinite(f));
        }
    }
}

TEST_CASE("MikkTSpace handles collapsed position bounds without casting NaN", "[asset][tangents]") {
    auto asset = mirrored_mesh();
    for (auto& v : asset.meshes[0].primitives[0].vertices) {
        v.position = {0, 0, 0};
    }
    auto result = engine::asset::generate_missing_tangents(asset);
    REQUIRE(result);
    for (const auto& v : asset.meshes[0].primitives[0].vertices) {
        for (float value : v.tangent) {
            CHECK(std::isfinite(value));
        }
    }
}

TEST_CASE("fuzzed absent position view is rejected before expensive tangent welding",
          "[asset][tangents][fuzz-regression]") {
    auto asset = engine::asset::import_glb_file(std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) /
                                                "fuzz_collapsed_position_view.glb");
    REQUIRE(asset);
    REQUIRE(asset->meshes.size() == 2U);
    REQUIRE(asset->meshes[1].primitives.size() == 1U);
    const auto& primitive = asset->meshes[1].primitives[0];
    REQUIRE(primitive.indices.size() == 3072U);
    REQUIRE(std::ranges::all_of(primitive.vertices, [](const auto& v) {
        return v.position == std::array<float, 3>{0, 0, 0};
    }));
    const auto before = geometry(*asset);
    const auto rejected = engine::asset::generate_missing_tangents(*asset);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == engine::core::ErrorCode::Validation);
    CHECK(rejected.error().details.at("limit") == engine::asset::kMaxTangentWeldPairs);
    CHECK(rejected.error().details.at("estimatedWork").get<std::uint64_t>() >
          engine::asset::kMaxTangentWeldPairs);
    CHECK(rejected.error().details.at("jsonPointer") == "/meshes/1/primitives/0");
    CHECK(geometry(*asset) == before);
}

TEST_CASE("tangent work is admitted for the entire asset before any mutation",
          "[asset][tangents][budget]") {
    // Each primitive costs 999*998/2 = 498501 pairs. Two fit; three do not.
    auto admitted = dense_asset(2, 999);
    REQUIRE(engine::asset::generate_missing_tangents(admitted));
    auto rejected = dense_asset(3, 999);
    const auto before = geometry(rejected);
    auto result = engine::asset::generate_missing_tangents(rejected);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == engine::core::ErrorCode::Validation);
    CHECK(result.error().details.at("primitiveIndex") == 2);
    CHECK(geometry(rejected) == before);

    auto across_meshes = dense_asset(1, 999);
    const auto repeated_mesh = across_meshes.meshes[0];
    across_meshes.meshes.assign(3, repeated_mesh);
    const auto across_before = geometry(across_meshes);
    result = engine::asset::generate_missing_tangents(across_meshes);
    REQUIRE_FALSE(result);
    CHECK(result.error().details.at("meshIndex") == 2);
    CHECK(geometry(across_meshes) == across_before);

    auto source = engine::asset::import_glb_file(std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) /
                                                 "fuzz_collapsed_position_view.glb");
    REQUIRE(source);
    auto& primitives = source->meshes[1].primitives;
    const auto repeated_primitive = primitives[0];
    primitives.assign(6, repeated_primitive);
    const auto six_before = geometry(*source);
    REQUIRE_FALSE(engine::asset::generate_missing_tangents(*source));
    CHECK(geometry(*source) == six_before);
}

TEST_CASE("tangent admission includes the exact budget and exempts 64-corner groups",
          "[asset][tangents][budget]") {
    auto asset = mirrored_mesh();
    // Independently chosen triangular numbers: 2080 + 185745 + 812175 = 1M.
    asset.meshes[0].primitives = {
        dense_group(65),
        dense_group(610),
        dense_group(1275),
        dense_group(64),
    };
    auto admitted = asset;
    REQUIRE(engine::asset::generate_missing_tangents(admitted));
    asset.meshes[0].primitives.push_back(dense_group(65));
    const auto before = geometry(asset);
    const auto rejected = engine::asset::generate_missing_tangents(asset);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().details.at("primitiveIndex") == 4);
    CHECK(rejected.error().details.at("estimatedWork").get<std::uint64_t>() ==
          engine::asset::kMaxTangentWeldPairs + 2080U);
    CHECK(geometry(asset) == before);
}

TEST_CASE("tangent work covers neighboring float positions at every grid boundary",
          "[asset][tangents][budget]") {
    for (std::uint32_t phase = 0; phase < 8U; ++phase) {
        auto asset = dense_asset(1, 1440);
        auto& vertices = asset.meshes[0].primitives[0].vertices;
        for (std::uint32_t v = 0; v < 8U; ++v) {
            for (std::uint32_t axis = 0; axis < 3U; ++axis) {
                const auto bits = 0x3F800000U + ((phase >> axis) & 1U) + ((v >> axis) & 1U);
                vertices[v].position[axis] = std::bit_cast<float>(bits);
            }
        }
        // Only the matching shifted grid combines all eight positions. Every
        // other grid's estimate is below the limit at this corner count.
        const auto before = geometry(asset);
        require_budget_error(engine::asset::generate_missing_tangents(asset));
        CHECK(geometry(asset) == before);
    }
}

TEST_CASE("tangent work handles signed zero and tiny Float32 positions",
          "[asset][tangents][budget]") {
    const auto minimum = std::numeric_limits<float>::min();
    const auto subnormal = std::numeric_limits<float>::denorm_min();
    const auto boundary = std::ldexp(1.0F, -103);
    const std::array ranges{
        std::array{-0.0F, 0.0F},
        std::array{-subnormal, subnormal},
        std::array{minimum, std::nextafter(minimum, 1.0F)},
        std::array{4.0F * minimum, 4.5F * minimum}, // Normal values with a subnormal span.
        std::array{std::nextafter(boundary, 0.0F), boundary},
        std::array{-1.0F, std::nextafter(-1.0F, 0.0F)},
        std::array{
            std::nextafter(std::numeric_limits<float>::max(), 0.0F),
            std::numeric_limits<float>::max(),
        },
    };
    for (const auto& range : ranges) {
        auto asset = dense_asset(1, 1440);
        auto& vertices = asset.meshes[0].primitives[0].vertices;
        for (std::size_t v = 0; v < vertices.size(); ++v) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                vertices[v].position[axis] = range[(v >> axis) & 1U];
            }
        }
        require_budget_error(engine::asset::generate_missing_tangents(asset));
    }
}

TEST_CASE("supplied tangents and non-normal materials retain their import behavior",
          "[asset][tangents][budget]") {
    auto supplied = dense_asset(6, 3072);
    for (auto& primitive : supplied.meshes[0].primitives) {
        for (auto& v : primitive.vertices) {
            v.tangent = {1, 0, 0, -1};
        }
    }
    const auto before = geometry(supplied);
    REQUIRE(engine::asset::generate_missing_tangents(supplied));
    CHECK(geometry(supplied) == before);
    auto flat = dense_asset(6, 3072);
    flat.materials[0].normal_texture.reset();
    const auto flat_before = geometry(flat);
    REQUIRE(engine::asset::generate_missing_tangents(flat));
    CHECK(geometry(flat) == flat_before);
}

TEST_CASE("non-finite positions are rejected before spatial budget accounting",
          "[asset][tangents][budget]") {
    const std::array values{
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
    };
    for (const float value : values) {
        auto asset = dense_asset(1, 1440);
        asset.meshes[0].primitives[0].vertices[0].position[0] = value;
        const auto before = geometry(asset);
        const auto invalid = engine::asset::generate_missing_tangents(asset);
        REQUIRE_FALSE(invalid);
        CHECK(invalid.error().code == engine::core::ErrorCode::Validation);
        CHECK(geometry(asset) == before);
    }
}

TEST_CASE("Blender mirrored UV fixture matches generated tangents on both islands",
          "[asset][tangents][mirrored-fixture]") {
    const auto fixtures = std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR);
    auto generated = engine::asset::import_glb_file(fixtures / "blender_mirrored_no_tangents.glb");
    auto reference = engine::asset::import_glb_file(fixtures / "blender_mirrored_tangents.glb");
    REQUIRE(generated);
    REQUIRE(reference);
    REQUIRE(engine::asset::generate_missing_tangents(*generated));
    REQUIRE(generated->meshes.size() == reference->meshes.size());
    std::size_t normal_mapped_primitives = 0;
    bool positive_sign = false;
    bool negative_sign = false;
    for (std::size_t mesh = 0; mesh < generated->meshes.size(); ++mesh) {
        const auto& actual_mesh = generated->meshes[mesh];
        const auto& expected_mesh = reference->meshes[mesh];
        REQUIRE(actual_mesh.primitives.size() == expected_mesh.primitives.size());
        for (std::size_t prim = 0; prim < actual_mesh.primitives.size(); ++prim) {
            const auto& actual = actual_mesh.primitives[prim];
            if (!actual.material.has_value() ||
                !generated->materials[*actual.material].normal_texture.has_value()) {
                continue;
            }
            ++normal_mapped_primitives;
            const auto& expected = expected_mesh.primitives[prim];
            REQUIRE(actual.indices.size() == 3072U);
            REQUIRE(actual.indices.size() == expected.indices.size());
            for (std::size_t corner = 0; corner < actual.indices.size(); ++corner) {
                const auto& a = actual.vertices[actual.indices[corner]];
                const auto& b = expected.vertices[expected.indices[corner]];
                CHECK(a.position == b.position);
                CHECK(a.uv0 == b.uv0);
                for (std::size_t axis = 0; axis < 4; ++axis) {
                    CHECK(std::abs(a.tangent[axis] - b.tangent[axis]) < 0.0002F);
                }
                positive_sign = positive_sign || a.tangent[3] == 1.0F;
                negative_sign = negative_sign || a.tangent[3] == -1.0F;
            }
        }
    }
    CHECK(normal_mapped_primitives == 1U);
    CHECK(positive_sign);
    CHECK(negative_sign);
}
