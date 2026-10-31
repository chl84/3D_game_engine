#include <engine/asset/gltf_import.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using engine::asset::ImportedAsset;
using engine::asset::Node;
using engine::core::ErrorCode;

constexpr std::uint32_t kJsonChunk = 0x4E4F534AU;
constexpr std::uint32_t kBinChunk = 0x004E4942U;

[[nodiscard]] std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    const std::string content{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(content.size());
    std::ranges::transform(content, bytes.begin(),
                           [](char value) { return static_cast<std::byte>(value); });
    return bytes;
}

[[nodiscard]] std::filesystem::path fixture(std::string_view name) {
    return std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / std::string(name);
}

[[nodiscard]] std::span<const std::byte> as_span(const std::vector<std::byte>& bytes) {
    return std::span<const std::byte>(bytes.data(), bytes.size());
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    return std::to_integer<std::uint32_t>(bytes[offset]) |
           (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void write_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

struct GlbChunks {
    std::string json;
    std::vector<std::byte> binary;
};

[[nodiscard]] GlbChunks split_glb(const std::vector<std::byte>& bytes) {
    REQUIRE(bytes.size() >= 20U);
    REQUIRE(bytes[0] == std::byte{'g'});
    REQUIRE(bytes[1] == std::byte{'l'});
    REQUIRE(bytes[2] == std::byte{'T'});
    REQUIRE(bytes[3] == std::byte{'F'});
    const std::uint32_t total = read_u32(as_span(bytes), 8U);
    REQUIRE(total <= bytes.size());
    GlbChunks chunks;
    std::size_t offset = 12U;
    while (offset + 8U <= total) {
        const std::uint32_t length = read_u32(as_span(bytes), offset);
        const std::uint32_t type = read_u32(as_span(bytes), offset + 4U);
        REQUIRE(offset + 8U + length <= bytes.size());
        if (type == kJsonChunk) {
            chunks.json.assign(reinterpret_cast<const char*>(bytes.data() + offset + 8U), length);
        } else if (type == kBinChunk) {
            chunks.binary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 8U),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(offset + 8U + length));
        }
        offset += 8U + length;
    }
    return chunks;
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    const auto offset = bytes.size();
    bytes.resize(bytes.size() + 4U);
    write_u32(bytes, offset, value);
}

[[nodiscard]] std::vector<std::byte> compose_glb(std::string_view json,
                                                 const std::vector<std::byte>& binary) {
    std::string padded_json(json);
    while ((padded_json.size() % 4U) != 0U) {
        padded_json.push_back(' ');
    }
    std::vector<std::byte> padded_binary = binary;
    while ((padded_binary.size() % 4U) != 0U) {
        padded_binary.push_back(std::byte{0});
    }
    const bool has_binary = !binary.empty();
    const std::uint32_t total = static_cast<std::uint32_t>(
        12U + 8U + padded_json.size() + (has_binary ? 8U + padded_binary.size() : 0U));
    std::vector<std::byte> bytes;
    bytes.reserve(total);
    bytes.insert(bytes.end(), {std::byte{'g'}, std::byte{'l'}, std::byte{'T'}, std::byte{'F'}});
    append_u32(bytes, 2U);
    append_u32(bytes, total);
    append_u32(bytes, static_cast<std::uint32_t>(padded_json.size()));
    append_u32(bytes, kJsonChunk);
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(padded_json.data()),
                 reinterpret_cast<const std::byte*>(padded_json.data() + padded_json.size()));
    if (has_binary) {
        append_u32(bytes, static_cast<std::uint32_t>(padded_binary.size()));
        append_u32(bytes, kBinChunk);
        bytes.insert(bytes.end(), padded_binary.begin(), padded_binary.end());
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte>
mutate_json(const std::vector<std::byte>& source,
            const std::function<void(nlohmann::json&)>& mutator) {
    auto chunks = split_glb(source);
    auto document = nlohmann::json::parse(chunks.json);
    mutator(document);
    return compose_glb(document.dump(), chunks.binary);
}

[[nodiscard]] const Node& node_named(const ImportedAsset& asset, std::string_view name) {
    const auto found =
        std::ranges::find_if(asset.nodes, [name](const Node& node) { return node.name == name; });
    REQUIRE(found != asset.nodes.end());
    return *found;
}

[[nodiscard]] std::size_t index_named(const ImportedAsset& asset, std::string_view name) {
    const auto found =
        std::ranges::find_if(asset.nodes, [name](const Node& node) { return node.name == name; });
    REQUIRE(found != asset.nodes.end());
    return static_cast<std::size_t>(std::distance(asset.nodes.begin(), found));
}

[[nodiscard]] bool close(float left, float right, float tolerance = 1.0e-5F) {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool close_d(double left, double right, double tolerance = 1.0e-6) {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool close_rotation(const engine::core::Quat& left, const engine::core::Quat& right) {
    const float dot =
        (left.x * right.x) + (left.y * right.y) + (left.z * right.z) + (left.w * right.w);
    return std::abs(dot) >= 0.99999F;
}

} // namespace

TEST_CASE("Blender GLB imports meshes, materials and textures", "[asset][gltf]") {
    auto asset = engine::asset::import_glb_file(fixture("blender_cube.glb"));
    REQUIRE(asset.has_value());

    CHECK(asset->nodes.size() == 3);
    CHECK(asset->scene_roots.size() == 1);
    const Node& root = node_named(*asset, "Root");
    const Node& pivot = node_named(*asset, "Pivot");
    const Node& cube = node_named(*asset, "Cube");
    CHECK_FALSE(root.parent.has_value());
    REQUIRE(pivot.parent.has_value());
    REQUIRE(cube.parent.has_value());
    CHECK(asset->nodes[*pivot.parent].name == "Root"); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(asset->nodes[*cube.parent].name == "Pivot"); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(root.path == "Root");
    CHECK(pivot.path == "Root/Pivot");
    CHECK(cube.path == "Root/Pivot/Cube");

    REQUIRE(asset->meshes.size() == 1);
    const auto& mesh = asset->meshes.front();
    CHECK(mesh.name == "CubeMesh");
    REQUIRE(mesh.primitives.size() == 1);
    const auto& primitive = mesh.primitives.front();
    CHECK(primitive.vertices.size() == 24);
    CHECK(primitive.indices.size() == 36);
    REQUIRE(primitive.material.has_value());
    CHECK(*primitive.material == 0U); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(primitive.bounds.min[0] <= primitive.bounds.max[0]);
    CHECK(primitive.bounds.min[1] <= primitive.bounds.max[1]);
    CHECK(primitive.bounds.min[2] <= primitive.bounds.max[2]);

    REQUIRE(asset->materials.size() == 1);
    const auto& material = asset->materials.front();
    CHECK(material.name == "M3Painted");
    CHECK(close(material.metallic_factor, 0.25F));
    CHECK(close(material.roughness_factor, 0.6F));
    REQUIRE(material.base_color_texture.has_value());
    CHECK(material.base_color_texture->texture == 0U); // NOLINT(bugprone-unchecked-optional-access)

    REQUIRE(asset->textures.size() == 1);
    REQUIRE(asset->textures.front().image.has_value());
    CHECK(*asset->textures.front().image == 0U); // NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(asset->images.size() == 1);
    CHECK(asset->images.front().mime_type == engine::asset::ImageMimeType::Png);
    CHECK(!asset->images.front().bytes.empty());

    REQUIRE(asset->samplers.size() == 1);
    CHECK(asset->samplers.front().mag_filter == engine::asset::Filter::Linear);
    CHECK(asset->samplers.front().min_filter == engine::asset::Filter::LinearMipmapLinear);
    CHECK(asset->samplers.front().wrap_s == engine::asset::Wrap::Repeat);
    CHECK(asset->samplers.front().wrap_t == engine::asset::Wrap::Repeat);
    CHECK(asset->animation_channel_count == 0U);
}

TEST_CASE("animation channels are reported and not imported", "[asset][gltf]") {
    auto still = engine::asset::import_glb_file(fixture("blender_cube.glb"));
    auto animated = engine::asset::import_glb_file(fixture("blender_animated.glb"));
    REQUIRE(still.has_value());
    REQUIRE(animated.has_value());
    CHECK(still->animation_channel_count == 0U);
    CHECK(animated->animation_channel_count == 1U);
    CHECK(animated->nodes.size() == still->nodes.size());
    CHECK(animated->meshes.size() == still->meshes.size());

    const auto source = read_bytes(fixture("blender_cube.glb"));
    auto imported = engine::asset::import_glb(as_span(source), "memory.glb");
    REQUIRE(imported.has_value());
    CHECK(imported->source == "memory.glb");
    CHECK(imported->meshes.size() == 1);
}

TEST_CASE("node transforms compose in double precision", "[asset][gltf]") {
    auto asset = engine::asset::import_glb_file(fixture("blender_cube.glb"));
    REQUIRE(asset.has_value());
    const auto world = engine::asset::world_transforms(*asset);
    REQUIRE(world.size() == asset->nodes.size());
    const auto cube_index = index_named(*asset, "Cube");
    const auto& cube_world = world[cube_index];
    CHECK(close_d(cube_world.position.x, 1.0));
    CHECK(close_d(cube_world.position.y, -1.75));
    CHECK(close_d(cube_world.position.z, -0.8660254037844386));
    CHECK(close(cube_world.scale.x, 0.5F));
    CHECK(close(cube_world.scale.y, 1.25F));
    CHECK(close(cube_world.scale.z, 0.75F, 1.0e-6F));
    const engine::core::Quat expected{0.16773126F, 0.25488700F, -0.04494345F, 0.95125124F};
    CHECK(close_rotation(cube_world.rotation, expected));

    ImportedAsset synthetic;
    synthetic.nodes.resize(2);
    synthetic.nodes[0].name = "Root";
    synthetic.nodes[0].translation = {1.0e9, 0.0, 0.0};
    synthetic.nodes[0].rotation = {0.0F, 0.0F, 0.70710678F, 0.70710678F};
    synthetic.nodes[0].scale = {2.0F, 2.0F, 2.0F};
    synthetic.nodes[0].children = {1U};
    synthetic.nodes[1].name = "Child";
    synthetic.nodes[1].parent = 0U;
    synthetic.nodes[1].translation = {0.0, 1.0, 0.0};
    const auto synthetic_world = engine::asset::world_transforms(synthetic);
    REQUIRE(synthetic_world.size() == 2);
    CHECK(close_d(synthetic_world[1].position.x, 1.0e9 - 2.0));
    CHECK(close_d(synthetic_world[1].position.y, 0.0));
    CHECK(close_d(synthetic_world[1].position.z, 0.0));
    CHECK(close(synthetic_world[1].scale.x, 2.0F));
}

TEST_CASE("malformed and unsupported GLB input is rejected with stable errors", "[asset][gltf]") {
    const auto valid = read_bytes(fixture("blender_cube.glb"));

    SECTION("binary chunk shorter than the declared buffer") {
        auto chunks = split_glb(valid);
        chunks.binary.resize(16U);
        const auto bytes = compose_glb(chunks.json, chunks.binary);
        const auto result = engine::asset::import_glb(as_span(bytes), "short-bin.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/buffers/0");
    }

    SECTION("truncated container") {
        auto truncated = valid;
        truncated.resize(100U);
        const auto result = engine::asset::import_glb(as_span(truncated), "truncated.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "");
    }

    SECTION("JSON glTF container") {
        const std::string json = R"({"asset":{"version":"2.0"}})";
        std::vector<std::byte> bytes(json.size());
        std::ranges::transform(json, bytes.begin(),
                               [](char value) { return static_cast<std::byte>(value); });
        const auto result = engine::asset::import_glb(as_span(bytes), "scene.gltf");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
    }

    SECTION("invalid JSON") {
        const std::string json = R"({"asset":{"version":)";
        const auto bytes = compose_glb(json, {});
        const auto result = engine::asset::import_glb(as_span(bytes), "broken.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "");
    }

    SECTION("required extension") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["extensionsRequired"] = nlohmann::json::array({"KHR_draco_mesh_compression"});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "draco.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/extensionsRequired");
    }

    SECTION("optional extension that the importer does not implement") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["extensionsUsed"] = nlohmann::json::array({"KHR_texture_transform"});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "transform.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/extensionsUsed");
    }

    SECTION("blended material") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["materials"][0]["alphaMode"] = "BLEND";
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "blend.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/materials/0");
    }

    SECTION("missing POSITION attribute") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["meshes"][0]["primitives"][0]["attributes"].erase("POSITION");
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "no-position.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("unsupported vertex attribute") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& attributes = document["meshes"][0]["primitives"][0]["attributes"];
            attributes["COLOR_0"] = attributes["NORMAL"];
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "color.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
    }

    SECTION("external buffer") {
        const auto bytes = mutate_json(
            valid, [](nlohmann::json& document) { document["buffers"][0]["uri"] = "mesh.bin"; });
        const auto result = engine::asset::import_glb(as_span(bytes), "external-buffer.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/buffers/0");
    }

    SECTION("missing binary chunk") {
        const auto chunks = split_glb(valid);
        const auto bytes = compose_glb(chunks.json, {});
        const auto result = engine::asset::import_glb(as_span(bytes), "missing-buffer.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
    }

    SECTION("multiple scenes") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["scenes"].push_back(nlohmann::json{{"nodes", {2}}});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "scenes.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
    }

    SECTION("non-indexed primitive") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["meshes"][0]["primitives"][0].erase("indices");
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "non-indexed.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("non-indexed primitive with invalid POSITION accessor") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            primitive.erase("indices");
            primitive["attributes"]["POSITION"] = 100000000;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "bad-position.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("non-indexed primitive with POSITION 999") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            primitive.erase("indices");
            primitive["attributes"]["POSITION"] = 999;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "position-999.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("non-indexed primitive with huge vertex count") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            primitive.erase("indices");
            document["accessors"][primitive["attributes"]["POSITION"].get<std::size_t>()]["count"] =
                std::uint64_t{1} << 62U;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "huge-count.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/accessors/0");
    }

    SECTION("non-triangle primitive") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["meshes"][0]["primitives"][0]["mode"] = 1;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "lines.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("skinned mesh") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["skins"] = nlohmann::json::array({
                {{"joints", {0}}},
            });
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "skin.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/skins");
    }

    SECTION("camera") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["cameras"] = nlohmann::json::array({
                {
                    {"type", "perspective"},
                    {"perspective", {{"yfov", 0.7}, {"znear", 0.1}, {"zfar", 100.0}}},
                },
            });
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "camera.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/cameras");
    }

    SECTION("punctual light extension") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["extensionsUsed"] = nlohmann::json::array({"KHR_lights_punctual"});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "lights.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/extensionsUsed");
    }

    SECTION("morph targets") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            const auto position = primitive["attributes"]["POSITION"];
            primitive["targets"] = nlohmann::json::array({
                {{"POSITION", position}},
            });
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "morph.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0/targets");
    }

    SECTION("non-float vertex data") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            auto& accessor =
                document["accessors"][primitive["attributes"]["POSITION"].get<std::size_t>()];
            accessor["componentType"] = 5121;
            accessor["normalized"] = true;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "quantized.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("mesh morph weights without targets") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["meshes"][0]["weights"] = nlohmann::json::array({0.0});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "mesh-weights.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/weights");
    }

    SECTION("non-float texture coordinates") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& primitive = document["meshes"][0]["primitives"][0];
            auto& accessor =
                document["accessors"][primitive["attributes"]["TEXCOORD_0"].get<std::size_t>()];
            accessor["componentType"] = 5121;
            accessor["normalized"] = true;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "quantized-uv.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0");
    }

    SECTION("material factor outside the glTF range") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] = 1.5;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "metallic.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/materials/0");
    }

    SECTION("alpha cutoff above one is valid glTF") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["materials"][0]["alphaMode"] = "MASK";
            document["materials"][0]["alphaCutoff"] = 1.5;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "alpha-cutoff.glb");
        REQUIRE(result.has_value());
    }

    SECTION("non-unit node rotation") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["nodes"][0]["rotation"] = nlohmann::json::array({0.0, 0.0, 0.0, 2.0});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "rotation.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/nodes/0");
    }

    SECTION("zero node matrix") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            auto& node = document["nodes"][0];
            node.erase("translation");
            node.erase("rotation");
            node.erase("scale");
            node["matrix"] = nlohmann::json::array(
                {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "matrix.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().details["pointer"] == "/nodes/0");
    }

    SECTION("invalid primitive material index") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["meshes"][0]["primitives"][0]["material"] = 99;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "material.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/meshes/0/primitives/0/material");
    }

    SECTION("default scene out of range") {
        const auto bytes =
            mutate_json(valid, [](nlohmann::json& document) { document["defaultScene"] = 99; });
        const auto result = engine::asset::import_glb(as_span(bytes), "scene.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        // fastgltf rejects this while parsing, before the importer's own check.
        CHECK(result.error().details.contains("pointer"));
    }

    SECTION("scene root out of range") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["scenes"][0]["nodes"] = nlohmann::json::array({99});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "root.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/scenes/0/nodes/99");
    }

    SECTION("accessor byte range overflow") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["accessors"][0]["count"] = std::uint64_t{1} << 62U;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "overflow.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/accessors/0");
    }

    SECTION("compressed texture image") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["images"][0]["mimeType"] = "image/ktx2";
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "texture.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
    }

    SECTION("animation sampler index") {
        const auto animated = read_bytes(fixture("blender_animated.glb"));
        const auto bytes = mutate_json(animated, [](nlohmann::json& document) {
            document["animations"][0]["channels"][0]["sampler"] = 99;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "animation.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/animations/0/channels/0/sampler");
    }

    SECTION("animation output accessor") {
        const auto animated = read_bytes(fixture("blender_animated.glb"));
        const auto bytes = mutate_json(animated, [](nlohmann::json& document) {
            document["animations"][0]["samplers"][0]["output"] = 999;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "animation.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/animations/0/samplers/0/output");
    }

    SECTION("animation target node") {
        const auto animated = read_bytes(fixture("blender_animated.glb"));
        const auto bytes = mutate_json(animated, [](nlohmann::json& document) {
            document["animations"][0]["channels"][0]["target"]["node"] = 99;
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "animation.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
        CHECK(result.error().details["pointer"] == "/animations/0/channels/0/target/node");
    }

    SECTION("external image") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["images"][0] =
                nlohmann::json{{"uri", "texture.png"}, {"mimeType", "image/png"}};
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "external-image.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Unsupported);
        CHECK(result.error().details["pointer"] == "/images/0");
    }

    SECTION("node hierarchy cycle") {
        const auto bytes = mutate_json(valid, [](nlohmann::json& document) {
            document["nodes"][0]["children"] = nlohmann::json::array({2});
        });
        const auto result = engine::asset::import_glb(as_span(bytes), "cycle.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
    }

    SECTION("index outside the vertex range") {
        auto chunks = split_glb(valid);
        auto document = nlohmann::json::parse(chunks.json);
        const auto accessor_index =
            document["meshes"][0]["primitives"][0]["indices"].get<std::size_t>();
        const auto& accessor = document["accessors"][accessor_index];
        const auto& view = document["bufferViews"][accessor["bufferView"].get<std::size_t>()];
        const std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
        REQUIRE(offset + 2U <= chunks.binary.size());
        const std::uint16_t bad_index = 60000U;
        std::memcpy(chunks.binary.data() + offset, &bad_index, sizeof(bad_index));
        const auto bytes = compose_glb(document.dump(), chunks.binary);
        const auto result = engine::asset::import_glb(as_span(bytes), "bad-index.glb");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::Validation);
    }
}
