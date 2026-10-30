#include <engine/asset/asset_library.hpp>
#include <engine/core/reflect.hpp>
#include <engine/scene/project.hpp>
#include <engine/scene/scene.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "temp_dir.hpp"

namespace {

using engine::asset::AssetRecord;
using engine::asset::import_asset;
using engine::asset::list_assets;
using engine::asset::load_asset;
using engine::asset::reimport_asset;
using engine::core::ErrorCode;
using engine::core::Uuid;

[[nodiscard]] std::filesystem::path fixture(std::string_view name) {
    return std::filesystem::path(ENGINE_ASSET_FIXTURE_DIR) / std::string(name);
}

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

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void validate_schema(const std::filesystem::path& schema_path, const nlohmann::json& document) {
    const auto schema = nlohmann::json::parse(read_text(schema_path));
    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(schema);
    CHECK_NOTHROW(validator.validate(document));
}

[[nodiscard]] std::map<std::string, std::string>
node_ids(const engine::asset::ImportedAsset& asset) {
    std::map<std::string, std::string> ids;
    for (const auto& node : asset.nodes) {
        ids[node.path] = node.id.to_string();
    }
    return ids;
}

[[nodiscard]] std::map<std::string, std::string>
mesh_ids(const engine::asset::ImportedAsset& asset) {
    std::map<std::string, std::string> ids;
    for (const auto& mesh : asset.meshes) {
        ids[mesh.name] = mesh.id.to_string();
    }
    return ids;
}

} // namespace

TEST_CASE("asset import writes sidecar and cooked cache", "[asset][library]") {
    engine::test::TempDir temp("asset-library-import");
    auto project = engine::scene::Project::create(temp.path() / "project", "Asset Test");
    REQUIRE(project.has_value());
    const auto source = temp.path() / "model.glb";
    std::filesystem::copy_file(fixture("blender_cube.glb"), source);

    auto imported = import_asset(project->root(), source, "Cube");
    REQUIRE(imported.has_value());
    CHECK_FALSE(imported->cache_hit);
    CHECK(imported->record.asset_id.is_nil() == false);
    CHECK(imported->record.source_hash.size() == 64U);
    CHECK(imported->record.cache_key.size() == 64U);
    CHECK(std::filesystem::is_regular_file(project->root() / "assets" /
                                           imported->record.asset_id.to_string() / "asset.json"));
    CHECK(std::filesystem::is_regular_file(project->root() / "assets" /
                                           imported->record.asset_id.to_string() / "source.glb"));
    CHECK(std::filesystem::is_regular_file(project->root() / imported->record.cache_path /
                                           "cooked.json"));
    CHECK(std::filesystem::is_regular_file(project->root() / imported->record.cache_path /
                                           "meshes.bin"));
    CHECK(std::filesystem::is_regular_file(project->root() / imported->record.cache_path /
                                           "textures.bin"));

    auto loaded = load_asset(project->root(), imported->record.asset_id);
    REQUIRE(loaded.has_value());
    CHECK(loaded->asset.nodes.size() == 3);
    CHECK(loaded->asset.meshes.size() == 1);
    CHECK(loaded->asset.materials.size() == 1);
    CHECK(loaded->textures.size() == 1);
    CHECK(loaded->textures.front().width == 4U);
    CHECK(loaded->textures.front().height == 4U);
    CHECK(loaded->textures.front().rgba8.size() == 64U);
    CHECK(loaded->textures.front().color_space == engine::asset::TextureColorSpace::Srgb);
    CHECK(loaded->asset.nodes.front().id.is_nil() == false);
    CHECK(loaded->asset.meshes.front().id.is_nil() == false);
    CHECK(loaded->asset.meshes.front().primitives.front().id.is_nil() == false);
    CHECK(loaded->asset.materials.front().id.is_nil() == false);
    CHECK(loaded->record.asset_id == imported->record.asset_id);

    validate_schema(
        std::filesystem::path(ENGINE_SCHEMA_DIR) / "asset@1.schema.json",
        nlohmann::json::parse(read_text(project->root() / "assets" /
                                        imported->record.asset_id.to_string() / "asset.json")));
    validate_schema(std::filesystem::path(ENGINE_SCHEMA_DIR) / "asset-cooked@1.schema.json",
                    nlohmann::json::parse(
                        read_text(project->root() / imported->record.cache_path / "cooked.json")));

    const auto listed = list_assets(project->root());
    REQUIRE(listed.has_value());
    CHECK(listed->size() == 1);
}

TEST_CASE("reimport preserves asset and stable IDs without changing bytes", "[asset][library]") {
    engine::test::TempDir temp("asset-library-reimport");
    auto project = engine::scene::Project::create(temp.path() / "project", "Asset Test");
    REQUIRE(project.has_value());
    const auto source = temp.path() / "model.glb";
    std::filesystem::copy_file(fixture("blender_cube.glb"), source);

    auto imported = import_asset(project->root(), source);
    if (!imported) {
        UNSCOPED_INFO("import failed: " << imported.error().message << " "
                                        << imported.error().details.dump());
    }
    REQUIRE(imported.has_value());
    auto first = load_asset(project->root(), imported->record.asset_id);
    REQUIRE(first.has_value());
    const auto first_nodes = node_ids(first->asset);
    const auto first_meshes = mesh_ids(first->asset);
    const auto sidecar_path =
        project->root() / "assets" / imported->record.asset_id.to_string() / "asset.json";
    const auto cooked_path = project->root() / imported->record.cache_path / "cooked.json";
    const auto meshes_path = project->root() / imported->record.cache_path / "meshes.bin";
    const auto textures_path = project->root() / imported->record.cache_path / "textures.bin";
    const auto sidecar_before = read_text(sidecar_path);
    const auto cooked_before = read_text(cooked_path);
    const auto meshes_before = read_bytes(meshes_path);
    const auto textures_before = read_bytes(textures_path);

    auto unchanged = reimport_asset(project->root(), imported->record.asset_id);
    REQUIRE(unchanged.has_value());
    CHECK(unchanged->cache_hit);
    CHECK(unchanged->record.asset_id == imported->record.asset_id);
    CHECK(unchanged->record.cache_key == imported->record.cache_key);
    CHECK(read_text(sidecar_path) == sidecar_before);
    CHECK(read_text(cooked_path) == cooked_before);
    CHECK(read_bytes(meshes_path) == meshes_before);
    CHECK(read_bytes(textures_path) == textures_before);

    auto second = load_asset(project->root(), imported->record.asset_id);
    REQUIRE(second.has_value());
    CHECK(node_ids(second->asset) == first_nodes);
    CHECK(mesh_ids(second->asset) == first_meshes);

    engine::core::ReflectionRegistry registry;
    engine::scene::register_scene_components(registry);
    engine::scene::Scene scene = engine::scene::Scene::with_seed(7);
    scene.set_scene_name("main");
    const auto entity = scene.create_entity("Model");
    REQUIRE(entity.has_value());
    REQUIRE(scene.add_component<engine::scene::AssetInstance>(*entity).has_value());
    scene.component<engine::scene::AssetInstance>(*entity).asset_id =
        imported->record.asset_id.to_string();
    REQUIRE(project->save_scene("scenes/main.scene.json", scene, registry).has_value());
    auto reference = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE(reference.has_value());
    const auto reference_entity = reference->find(scene.uuid(*entity).value_or(Uuid{}));
    REQUIRE(reference_entity.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(reference->component<engine::scene::AssetInstance>(*reference_entity).asset_id ==
          imported->record.asset_id.to_string());

    std::filesystem::copy_file(fixture("blender_animated.glb"), source,
                               std::filesystem::copy_options::overwrite_existing);
    auto changed = reimport_asset(project->root(), imported->record.asset_id);
    REQUIRE(changed.has_value());
    CHECK_FALSE(changed->cache_hit);
    CHECK(changed->record.asset_id == imported->record.asset_id);
    CHECK(changed->record.cache_key != imported->record.cache_key);
    auto third = load_asset(project->root(), imported->record.asset_id);
    REQUIRE(third.has_value());
    CHECK(third->asset.animation_channel_count == 1U);
    CHECK(node_ids(third->asset) == first_nodes);
    CHECK(mesh_ids(third->asset) == first_meshes);
    auto reference_after = project->load_scene("scenes/main.scene.json", registry);
    REQUIRE(reference_after.has_value());
    const auto after_entity = reference_after->find(scene.uuid(*entity).value_or(Uuid{}));
    REQUIRE(after_entity.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(reference_after->component<engine::scene::AssetInstance>(*after_entity).asset_id ==
          imported->record.asset_id.to_string());
}

TEST_CASE("cooking generates tangents for normal-mapped meshes", "[asset][library]") {
    engine::test::TempDir temp("asset-library-tangents");
    auto project = engine::scene::Project::create(temp.path() / "project", "Asset Test");
    REQUIRE(project.has_value());

    auto imported = import_asset(project->root(), fixture("normalmap.glb"));
    REQUIRE(imported.has_value());
    auto loaded = load_asset(project->root(), imported->record.asset_id);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->asset.meshes.size() == 1);
    REQUIRE(loaded->asset.meshes.front().primitives.size() == 1);
    const auto& primitive = loaded->asset.meshes.front().primitives.front();
    REQUIRE(loaded->asset.materials.size() == 1);
    CHECK(loaded->asset.materials.front().normal_texture.has_value());
    REQUIRE(primitive.vertices.size() == 3);
    for (const auto& vertex : primitive.vertices) {
        const float length_squared = (vertex.tangent[0] * vertex.tangent[0]) +
                                     (vertex.tangent[1] * vertex.tangent[1]) +
                                     (vertex.tangent[2] * vertex.tangent[2]);
        CHECK(std::abs(length_squared - 1.0F) < 1.0e-4F);
        // glTF's sign differs from MikkTSpace's native UV convention.
        CHECK(vertex.tangent[3] == -1.0F);
        CHECK(std::abs(vertex.tangent[0]) > 0.99F);
        CHECK(std::abs(vertex.tangent[1]) < 1.0e-4F);
        CHECK(std::abs(vertex.tangent[2]) < 1.0e-4F);
    }
}

TEST_CASE("asset library rejects missing projects and stale caches", "[asset][library]") {
    engine::test::TempDir temp("asset-library-errors");
    const auto source = temp.path() / "model.glb";
    std::filesystem::copy_file(fixture("blender_cube.glb"), source);
    const auto missing = import_asset(temp.path() / "missing", source);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == ErrorCode::NotFound);

    auto project = engine::scene::Project::create(temp.path() / "project", "Asset Test");
    REQUIRE(project.has_value());
    auto imported = import_asset(project->root(), source);
    if (!imported) {
        UNSCOPED_INFO("import failed: " << imported.error().message << " "
                                        << imported.error().details.dump());
    }
    REQUIRE(imported.has_value());
    const auto cooked = project->root() / imported->record.cache_path / "cooked.json";
    auto document = read_text(cooked);
    const auto key_position = document.find("\"cacheKey\"");
    REQUIRE(key_position != std::string::npos);
    document.replace(key_position, document.find('\n', key_position) - key_position,
                     R"("cacheKey": "stale")");
    write_text(cooked, document);
    const auto stale = load_asset(project->root(), imported->record.asset_id);
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code == ErrorCode::Validation);

    const auto sidecar =
        project->root() / "assets" / imported->record.asset_id.to_string() / "asset.json";
    auto sidecar_document = nlohmann::json::parse(read_text(sidecar));
    sidecar_document["sourceCopy"] = "../escape.glb";
    write_text(sidecar, sidecar_document.dump(2));
    const auto escaped = reimport_asset(project->root(), imported->record.asset_id);
    REQUIRE_FALSE(escaped.has_value());
    CHECK(escaped.error().code == ErrorCode::Validation);
    CHECK_FALSE(std::filesystem::exists(temp.path() / "escape.glb"));

    sidecar_document = nlohmann::json::parse(read_text(sidecar));
    sidecar_document["sourceCopy"] = (temp.path() / "absolute.glb").generic_string();
    write_text(sidecar, sidecar_document.dump(2));
    const auto absolute_source = reimport_asset(project->root(), imported->record.asset_id);
    REQUIRE_FALSE(absolute_source.has_value());
    CHECK(absolute_source.error().code == ErrorCode::Validation);
    CHECK_FALSE(std::filesystem::exists(temp.path() / "absolute.glb"));

    sidecar_document = nlohmann::json::parse(read_text(sidecar));
    sidecar_document["cooked"]["path"] = "../../escape-cache";
    write_text(sidecar, sidecar_document.dump(2));
    const auto escaped_load = load_asset(project->root(), imported->record.asset_id);
    REQUIRE_FALSE(escaped_load.has_value());
    CHECK(escaped_load.error().code == ErrorCode::Validation);

    sidecar_document = nlohmann::json::parse(read_text(sidecar));
    sidecar_document["cooked"]["path"] = (temp.path() / "absolute-cache").generic_string();
    write_text(sidecar, sidecar_document.dump(2));
    const auto absolute_load = load_asset(project->root(), imported->record.asset_id);
    REQUIRE_FALSE(absolute_load.has_value());
    CHECK(absolute_load.error().code == ErrorCode::Validation);
}
