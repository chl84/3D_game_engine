#include <engine/asset/gltf_import.hpp>
#include <engine/asset/tangents.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>

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
