#include <engine/asset/asset_library.hpp>

#include <engine/asset/image_decode.hpp>
#include <engine/asset/tangents.hpp>

#include <engine/core/path_sandbox.hpp>
#include <engine/core/sha256.hpp>
#include <engine/platform/atomic_file.hpp>

#include "asset_schemas.hpp"
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::asset {
namespace {

using core::Error;
using core::ErrorCode;
using core::Result;
using core::Uuid;
using nlohmann::json;

constexpr std::uint32_t kMeshMagic = 0x48534D45U;    // EMSH
constexpr std::uint32_t kTextureMagic = 0x58455445U; // ETEX
constexpr std::uint32_t kCookedVersion = 1U;
constexpr std::size_t kMeshHeaderSize = 16U;
constexpr std::size_t kTextureHeaderSize = 12U;

[[nodiscard]] Error asset_error(ErrorCode code, std::string message, std::string hint = {},
                                json details = json::object()) {
    return core::make_error(code, std::move(message), std::move(hint), std::move(details));
}

[[nodiscard]] Result<std::vector<std::byte>> read_file(const std::filesystem::path& path,
                                                       std::size_t limit) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return core::make_failure<std::vector<std::byte>>(asset_error(
            ec == std::errc::no_such_file_or_directory ? ErrorCode::NotFound : ErrorCode::Io,
            "cannot read asset file", ec.message()));
    }
    if (size > limit) {
        return core::make_failure<std::vector<std::byte>>(
            asset_error(ErrorCode::Validation, "file exceeds the size limit", {},
                        {{"path", path.generic_string()}, {"bytes", size}, {"limit", limit}}));
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return core::make_failure<std::vector<std::byte>>(
            asset_error(ErrorCode::Io, "cannot open '" + path.generic_string() + "'"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty()) {
        return core::make_failure<std::vector<std::byte>>(
            asset_error(ErrorCode::Io, "cannot read '" + path.generic_string() + "'"));
    }
    return bytes;
}

[[nodiscard]] Result<void> write_bytes(const std::filesystem::path& path,
                                       std::span<const std::byte> bytes) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return core::make_failure<void>(asset_error(
            ErrorCode::Io, "cannot create directory for '" + path.generic_string() + "'",
            ec.message()));
    }
    const auto text = std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return engine::platform::atomic_write_file(path, text);
}

[[nodiscard]] Result<void> write_json(const std::filesystem::path& path, const json& document) {
    const std::string text = document.dump(2) + "\n";
    return engine::platform::atomic_write_file(path, text);
}

[[nodiscard]] Result<std::filesystem::path>
resolve_inside_project(const std::filesystem::path& root, std::string_view relative,
                       std::string_view what) {
    auto sandbox = core::PathSandbox::create(root);
    if (!sandbox) {
        return core::make_failure<std::filesystem::path>(sandbox.error());
    }
    auto resolved = sandbox->resolve(relative);
    if (!resolved) {
        return core::make_failure<std::filesystem::path>(
            asset_error(ErrorCode::Validation, "asset sidecar path escapes the project root",
                        "Reimport or recreate the asset sidecar.", {{"field", std::string(what)}}));
    }
    return *resolved;
}

[[nodiscard]] std::filesystem::path asset_dir(const std::filesystem::path& root,
                                              const Uuid& asset_id) {
    return root / "assets" / asset_id.to_string();
}

[[nodiscard]] std::filesystem::path sidecar_path(const std::filesystem::path& root,
                                                 const Uuid& asset_id) {
    return asset_dir(root, asset_id) / "asset.json";
}

[[nodiscard]] std::filesystem::path cache_root(const std::filesystem::path& root,
                                               const Uuid& asset_id) {
    return root / ".engine" / "cache" / asset_id.to_string();
}

[[nodiscard]] std::string stored_source(const std::filesystem::path& root,
                                        const std::filesystem::path& source) {
    std::error_code ec;
    const auto canonical_source = std::filesystem::weakly_canonical(source, ec);
    if (!ec) {
        const auto relative = std::filesystem::relative(canonical_source, root, ec);
        if (!ec && !relative.empty()) {
            return relative.generic_string();
        }
    }
    return source.generic_string();
}

[[nodiscard]] std::filesystem::path resolve_source(const std::filesystem::path& root,
                                                   std::string_view source,
                                                   std::string_view source_copy) {
    std::filesystem::path original(source);
    if (original.is_relative()) {
        original = root / original;
    }
    std::error_code ec;
    if (std::filesystem::is_regular_file(original, ec) && !ec) {
        return original;
    }
    if (auto copy = resolve_inside_project(root, source_copy, "sourceCopy");
        copy && std::filesystem::is_regular_file(*copy, ec) && !ec) {
        return *copy;
    }
    return {};
}

[[nodiscard]] Result<json> read_sidecar_json(const std::filesystem::path& root,
                                             const Uuid& asset_id) {
    auto path =
        resolve_inside_project(root, "assets/" + asset_id.to_string() + "/asset.json", "asset");
    if (!path) {
        return core::make_failure<json>(path.error());
    }
    auto bytes = read_file(*path, std::size_t{16} * 1024U * 1024U);
    if (!bytes) {
        return core::make_failure<json>(bytes.error());
    }
    try {
        auto document =
            json::parse(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        nlohmann::json_schema::json_validator validator(json::parse(detail::kAssetSchema));
        validator.validate(document);
        if (document.at("assetId") != asset_id.to_string()) {
            return core::make_failure<json>(asset_error(
                ErrorCode::Validation, "sidecar asset ID does not match its directory"));
        }
        for (const auto* key : {"sourceCopy", "cooked.path"}) {
            const auto relative = std::string_view(key) == "sourceCopy"
                                      ? document.at("sourceCopy").get<std::string>()
                                      : document.at("cooked").at("path").get<std::string>();
            auto resolved = resolve_inside_project(root, relative, key);
            if (!resolved) {
                return core::make_failure<json>(resolved.error());
            }
        }
        return document;
    } catch (const std::exception& error) {
        return core::make_failure<json>(
            asset_error(ErrorCode::Validation, "asset sidecar is not valid JSON", error.what()));
    }
}

[[nodiscard]] Result<AssetRecord> record_from_sidecar(const json& document, const Uuid& asset_id) {
    try {
        AssetRecord record;
        record.asset_id = asset_id;
        record.name = document.at("name").get<std::string>();
        record.source = document.at("source").get<std::string>();
        record.source_copy = document.at("sourceCopy").get<std::string>();
        record.source_hash = document.at("sourceHash").get<std::string>();
        record.cache_key = document.at("cooked").at("key").get<std::string>();
        record.cache_path = document.at("cooked").at("path").get<std::string>();
        record.animation_channel_count = document.value("animationChannelCount", 0U);
        return record;
    } catch (const json::exception& error) {
        return core::make_failure<AssetRecord>(asset_error(
            ErrorCode::Validation, "asset sidecar is missing required fields", error.what()));
    }
}

[[nodiscard]] Uuid parse_uuid(const json& document, const char* key) {
    const auto parsed = Uuid::parse(document.at(key).get<std::string>());
    if (!parsed) {
        throw json::type_error::create(302, std::string("invalid UUID in ") + key, &document);
    }
    return *parsed;
}

[[nodiscard]] std::string_view color_space_name(TextureColorSpace color_space) {
    return color_space == TextureColorSpace::Srgb ? "srgb" : "linear";
}

[[nodiscard]] TextureColorSpace parse_color_space(const json& document) {
    return document.get<std::string>() == "srgb" ? TextureColorSpace::Srgb
                                                 : TextureColorSpace::Linear;
}

[[nodiscard]] json sampler_json(const Sampler& sampler) {
    return {
        {"magFilter", static_cast<std::uint16_t>(sampler.mag_filter)},
        {"minFilter", static_cast<std::uint16_t>(sampler.min_filter)},
        {"wrapS", static_cast<std::uint16_t>(sampler.wrap_s)},
        {"wrapT", static_cast<std::uint16_t>(sampler.wrap_t)},
    };
}

[[nodiscard]] Sampler parse_sampler(const json& document) {
    Sampler sampler;
    sampler.mag_filter = static_cast<Filter>(document.value("magFilter", 9729));
    sampler.min_filter = static_cast<Filter>(document.value("minFilter", 9987));
    sampler.wrap_s = static_cast<Wrap>(document.value("wrapS", 10497));
    sampler.wrap_t = static_cast<Wrap>(document.value("wrapT", 10497));
    return sampler;
}

[[nodiscard]] json bounds_json(const Bounds& bounds) {
    return {{"min", bounds.min}, {"max", bounds.max}};
}

[[nodiscard]] json pbr_json(const Material& material) {
    return {
        {"baseColorFactor", material.base_color_factor},
        {"metallicFactor", material.metallic_factor},
        {"roughnessFactor", material.roughness_factor},
        {"emissiveFactor", material.emissive_factor},
        {"alphaMode", material.alpha_mode == AlphaMode::Mask ? "MASK" : "OPAQUE"},
        {"alphaCutoff", material.alpha_cutoff},
        {"doubleSided", material.double_sided},
        {"textures", json::object()},
    };
}

void assign_new_ids(ImportedAsset& asset) {
    for (auto& node : asset.nodes) {
        node.id = Uuid::generate();
    }
    for (auto& mesh : asset.meshes) {
        mesh.id = Uuid::generate();
        for (auto& primitive : mesh.primitives) {
            primitive.id = Uuid::generate();
        }
    }
    for (auto& material : asset.materials) {
        material.id = Uuid::generate();
    }
    for (auto& texture : asset.textures) {
        texture.id = Uuid::generate();
    }
}

void assign_existing_ids(ImportedAsset& asset, const json& sidecar) {
    const auto old_nodes = sidecar.value("nodes", json::array());
    const auto old_meshes = sidecar.value("meshes", json::array());
    const auto old_materials = sidecar.value("materials", json::array());
    const auto old_textures = sidecar.value("textures", json::array());

    std::unordered_map<std::string, Uuid> node_names;
    for (const auto& node : old_nodes) {
        if (node.contains("name") && node.contains("id")) {
            node_names.emplace(node["name"].get<std::string>(), parse_uuid(node, "id"));
        }
    }
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        auto& node = asset.nodes[index];
        if (index < old_nodes.size() && old_nodes[index].value("name", "") == node.name) {
            node.id = parse_uuid(old_nodes[index], "id");
        } else if (const auto found = node_names.find(node.name); found != node_names.end()) {
            node.id = found->second;
        } else {
            node.id = Uuid::generate();
        }
    }

    std::unordered_map<std::string, Uuid> mesh_names;
    for (const auto& mesh : old_meshes) {
        if (mesh.contains("name") && mesh.contains("id")) {
            mesh_names.emplace(mesh["name"].get<std::string>(), parse_uuid(mesh, "id"));
        }
    }
    for (std::size_t index = 0; index < asset.meshes.size(); ++index) {
        auto& mesh = asset.meshes[index];
        if (index < old_meshes.size() && old_meshes[index].value("name", "") == mesh.name) {
            mesh.id = parse_uuid(old_meshes[index], "id");
            const auto old_primitives = old_meshes[index].value("primitives", json::array());
            for (std::size_t primitive = 0; primitive < mesh.primitives.size(); ++primitive) {
                mesh.primitives[primitive].id = primitive < old_primitives.size()
                                                    ? parse_uuid(old_primitives[primitive], "id")
                                                    : Uuid::generate();
            }
        } else if (const auto found = mesh_names.find(mesh.name); found != mesh_names.end()) {
            mesh.id = found->second;
            for (auto& primitive : mesh.primitives) {
                primitive.id = Uuid::generate();
            }
        } else {
            mesh.id = Uuid::generate();
            for (auto& primitive : mesh.primitives) {
                primitive.id = Uuid::generate();
            }
        }
    }

    std::unordered_map<std::string, Uuid> material_names;
    for (const auto& material : old_materials) {
        if (material.contains("name") && material.contains("id")) {
            material_names.emplace(material["name"].get<std::string>(), parse_uuid(material, "id"));
        }
    }
    for (std::size_t index = 0; index < asset.materials.size(); ++index) {
        auto& material = asset.materials[index];
        if (index < old_materials.size() &&
            old_materials[index].value("name", "") == material.name) {
            material.id = parse_uuid(old_materials[index], "id");
        } else if (const auto found = material_names.find(material.name);
                   found != material_names.end()) {
            material.id = found->second;
        } else {
            material.id = Uuid::generate();
        }
    }

    for (std::size_t index = 0; index < asset.textures.size(); ++index) {
        asset.textures[index].id =
            index < old_textures.size() ? parse_uuid(old_textures[index], "id") : Uuid::generate();
    }
}

[[nodiscard]] Result<void> assign_texture_color_spaces(ImportedAsset& asset) {
    for (auto& texture : asset.textures) {
        texture.color_space = TextureColorSpace::Linear;
    }
    std::vector<std::uint8_t> usage(asset.textures.size(), 0);
    const auto mark = [&](const std::optional<TextureReference>& reference, bool srgb) {
        if (!reference.has_value()) {
            return;
        }
        usage[reference->texture] |= srgb ? 1U : 2U;
    };
    for (const auto& material : asset.materials) {
        mark(material.base_color_texture, true);
        mark(material.metallic_roughness_texture, false);
        mark(material.normal_texture, false);
        mark(material.occlusion_texture, false);
        mark(material.emissive_texture, true);
    }
    for (std::size_t index = 0; index < usage.size(); ++index) {
        if (usage[index] == 3U) {
            return core::make_failure<void>(asset_error(
                ErrorCode::Validation,
                "a texture is used as both sRGB and linear data in the same material set",
                "Use separate glTF textures for base color/emissive and data textures.",
                {{"texture", index}}));
        }
        asset.textures[index].color_space =
            usage[index] == 1U ? TextureColorSpace::Srgb : TextureColorSpace::Linear;
    }
    return {};
}

[[nodiscard]] json sidecar_document(const AssetRecord& record, const ImportedAsset& asset) {
    json nodes = json::array();
    std::vector<std::string> node_ids(asset.nodes.size());
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        node_ids[index] = asset.nodes[index].id.to_string();
    }
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        const auto& node = asset.nodes[index];
        json mesh_ids = json::array();
        if (node.mesh.has_value() && *node.mesh < asset.meshes.size()) {
            mesh_ids.push_back(asset.meshes[*node.mesh].id.to_string());
        }
        json parent_id = nullptr;
        if (node.parent.has_value() && *node.parent < node_ids.size()) {
            parent_id = node_ids[*node.parent];
        }
        nodes.push_back({
            {"id", node.id.to_string()},
            {"name", node.name},
            {"parentId", std::move(parent_id)},
            {"meshIds", std::move(mesh_ids)},
        });
    }

    json meshes = json::array();
    for (const auto& mesh : asset.meshes) {
        json primitives = json::array();
        for (const auto& primitive : mesh.primitives) {
            json material = nullptr;
            if (primitive.material.has_value() && *primitive.material < asset.materials.size()) {
                material = asset.materials[*primitive.material].id.to_string();
            }
            primitives.push_back({
                {"id", primitive.id.to_string()},
                {"materialId", std::move(material)},
                {"bounds", bounds_json(primitive.bounds)},
            });
        }
        meshes.push_back({
            {"id", mesh.id.to_string()},
            {"name", mesh.name},
            {"primitives", std::move(primitives)},
        });
    }

    json materials = json::array();
    for (const auto& material : asset.materials) {
        json textures = json::object();
        const auto texture_id = [&asset](const std::optional<TextureReference>& reference) -> json {
            if (!reference.has_value() || reference->texture >= asset.textures.size()) {
                return nullptr;
            }
            return asset.textures[reference->texture].id.to_string();
        };
        textures["baseColor"] = texture_id(material.base_color_texture);
        textures["metallicRoughness"] = texture_id(material.metallic_roughness_texture);
        textures["normal"] = texture_id(material.normal_texture);
        textures["occlusion"] = texture_id(material.occlusion_texture);
        textures["emissive"] = texture_id(material.emissive_texture);
        json pbr = pbr_json(material);
        pbr["textures"] = std::move(textures);
        materials.push_back({
            {"id", material.id.to_string()},
            {"name", material.name},
            {"pbr", std::move(pbr)},
        });
    }

    json textures = json::array();
    for (std::size_t index = 0; index < asset.textures.size(); ++index) {
        const auto& texture = asset.textures[index];
        const std::string source = texture.image.has_value()
                                       ? "source.glb#image/" + std::to_string(*texture.image)
                                       : "source.glb#texture/" + std::to_string(index);
        Sampler sampler;
        if (texture.sampler.has_value() && *texture.sampler < asset.samplers.size()) {
            sampler = asset.samplers[*texture.sampler];
        }
        textures.push_back({
            {"id", texture.id.to_string()},
            {"source", source},
            {"colorSpace", color_space_name(texture.color_space)},
            {"sampler", sampler_json(sampler)},
        });
    }

    json scene_roots = json::array();
    for (const auto root : asset.scene_roots) {
        scene_roots.push_back(asset.nodes.at(root).id.to_string());
    }
    return {
        {"formatVersion", 1},
        {"assetId", record.asset_id.to_string()},
        {"name", record.name},
        {"source", record.source},
        {"sourceCopy", record.source_copy},
        {"sourceHash", record.source_hash},
        {"importerVersion", kGltfImporterVersion},
        {"settings", json::object()},
        {"animationChannelCount", asset.animation_channel_count},
        {"nodes", std::move(nodes)},
        {"meshes", std::move(meshes)},
        {"materials", std::move(materials)},
        {"textures", std::move(textures)},
        {
            "dependencies",
            json::array(
                {{{"kind", "source"}, {"path", record.source_copy}, {"hash", record.source_hash}}}),
        },
        {"cooked", {{"key", record.cache_key}, {"path", record.cache_path}}},
        {"metadata", {{"sceneRoots", std::move(scene_roots)}}},
    };
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value) {
    output.push_back(static_cast<std::byte>(value & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void append_float(std::vector<std::byte>& output, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(output, bits);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> data, std::size_t offset) {
    if (offset + 4U > data.size()) {
        return 0;
    }
    return std::to_integer<std::uint32_t>(data[offset]) |
           (std::to_integer<std::uint32_t>(data[offset + 1U]) << 8U) |
           (std::to_integer<std::uint32_t>(data[offset + 2U]) << 16U) |
           (std::to_integer<std::uint32_t>(data[offset + 3U]) << 24U);
}

[[nodiscard]] float read_float(std::span<const std::byte> data, std::size_t offset) {
    const std::uint32_t bits = read_u32(data, offset);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_u32(std::vector<std::byte>& output, std::size_t offset, std::uint32_t value) {
    if (offset + 4U > output.size()) {
        return;
    }
    output[offset] = static_cast<std::byte>(value & 0xFFU);
    output[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    output[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    output[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

struct PrimitiveOffsets {
    std::size_t vertex_offset{0};
    std::size_t index_offset{0};
    std::uint32_t vertex_count{0};
    std::uint32_t index_count{0};
};

[[nodiscard]] Result<std::vector<std::vector<PrimitiveOffsets>>>
write_mesh_buffers(std::vector<std::byte>& output, const ImportedAsset& asset) {
    output.assign(kMeshHeaderSize, std::byte{0});
    std::vector<std::vector<PrimitiveOffsets>> offsets;
    offsets.reserve(asset.meshes.size());
    std::uint32_t primitive_count = 0;
    for (const auto& mesh : asset.meshes) {
        std::vector<PrimitiveOffsets> mesh_offsets;
        mesh_offsets.reserve(mesh.primitives.size());
        for (const auto& primitive : mesh.primitives) {
            if (primitive.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
                primitive.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
                return core::make_failure<std::vector<std::vector<PrimitiveOffsets>>>(
                    asset_error(ErrorCode::Validation, "primitive is too large to cook", {},
                                {
                                    {"vertices", primitive.vertices.size()},
                                    {"indices", primitive.indices.size()},
                                }));
            }
            PrimitiveOffsets primitive_offsets;
            primitive_offsets.vertex_offset = output.size();
            primitive_offsets.vertex_count = static_cast<std::uint32_t>(primitive.vertices.size());
            primitive_offsets.index_offset =
                output.size() + (primitive.vertices.size() * 12U * sizeof(float));
            primitive_offsets.index_count = static_cast<std::uint32_t>(primitive.indices.size());
            for (const auto& vertex : primitive.vertices) {
                for (const float value : vertex.position) {
                    append_float(output, value);
                }
                for (const float value : vertex.normal) {
                    append_float(output, value);
                }
                for (const float value : vertex.tangent) {
                    append_float(output, value);
                }
                for (const float value : vertex.uv0) {
                    append_float(output, value);
                }
            }
            for (const std::uint32_t index : primitive.indices) {
                append_u32(output, index);
            }
            mesh_offsets.push_back(primitive_offsets);
            ++primitive_count;
        }
        offsets.push_back(std::move(mesh_offsets));
    }
    write_u32(output, 0U, kMeshMagic);
    write_u32(output, 4U, kCookedVersion);
    write_u32(output, 8U, primitive_count);
    write_u32(output, 12U, 0U);
    return offsets;
}

[[nodiscard]] Result<std::vector<CookedTexture>> cook_textures(const ImportedAsset& asset) {
    std::vector<CookedTexture> cooked;
    cooked.reserve(asset.textures.size());
    for (std::size_t index = 0; index < asset.textures.size(); ++index) {
        const auto& texture = asset.textures[index];
        if (!texture.image.has_value() || *texture.image >= asset.images.size()) {
            return core::make_failure<std::vector<CookedTexture>>(asset_error(
                ErrorCode::Validation, "texture has no image to cook", {}, {{"texture", index}}));
        }
        const auto& image = asset.images[*texture.image];
        auto decoded = decode_image(image.bytes, image.mime_type);
        if (!decoded) {
            return core::make_failure<std::vector<CookedTexture>>(decoded.error());
        }
        CookedTexture result;
        result.id = texture.id;
        result.texture_id = texture.id;
        result.name = texture.name;
        result.color_space = texture.color_space;
        result.width = decoded->width;
        result.height = decoded->height;
        result.rgba8 = std::move(decoded->rgba8);
        if (texture.sampler.has_value() && *texture.sampler < asset.samplers.size()) {
            result.sampler = asset.samplers[*texture.sampler];
        }
        cooked.push_back(std::move(result));
    }
    return cooked;
}

[[nodiscard]] Result<std::vector<std::byte>>
write_texture_buffers(const std::vector<CookedTexture>& textures) {
    std::vector<std::byte> output(kTextureHeaderSize, std::byte{0});
    for (const auto& texture : textures) {
        if (texture.rgba8.size() > std::numeric_limits<std::uint32_t>::max()) {
            return core::make_failure<std::vector<std::byte>>(
                asset_error(ErrorCode::Validation, "decoded texture is too large to cook"));
        }
        append_u32(output, texture.width);
        append_u32(output, texture.height);
        append_u32(output, static_cast<std::uint32_t>(texture.rgba8.size()));
        for (const std::uint8_t value : texture.rgba8) {
            output.push_back(static_cast<std::byte>(value));
        }
    }
    write_u32(output, 0U, kTextureMagic);
    write_u32(output, 4U, kCookedVersion);
    write_u32(output, 8U, static_cast<std::uint32_t>(textures.size()));
    return output;
}

[[nodiscard]] json cooked_material_json(const Material& material, const ImportedAsset& asset) {
    json textures = json::object();
    const auto add = [&textures, &asset](const char* key,
                                         const std::optional<TextureReference>& reference) {
        if (reference.has_value() && reference->texture < asset.textures.size()) {
            textures[key] = asset.textures[reference->texture].id.to_string();
        } else {
            textures[key] = nullptr;
        }
    };
    add("baseColor", material.base_color_texture);
    add("metallicRoughness", material.metallic_roughness_texture);
    add("normal", material.normal_texture);
    add("occlusion", material.occlusion_texture);
    add("emissive", material.emissive_texture);
    json pbr = pbr_json(material);
    pbr["textures"] = std::move(textures);
    return {
        {"id", material.id.to_string()},
        {"name", material.name},
        {"pbr", std::move(pbr)},
        {"normalScale", material.normal_scale},
        {"occlusionStrength", material.occlusion_strength},
    };
}

[[nodiscard]] json
cooked_document(const AssetRecord& record, const ImportedAsset& asset,
                const std::vector<std::vector<PrimitiveOffsets>>& primitive_offsets,
                const std::vector<CookedTexture>& textures) {
    json nodes = json::array();
    std::vector<std::string> node_ids;
    node_ids.reserve(asset.nodes.size());
    for (const auto& node : asset.nodes) {
        node_ids.push_back(node.id.to_string());
    }
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        const auto& node = asset.nodes[index];
        json parent = nullptr;
        if (node.parent.has_value() && *node.parent < node_ids.size()) {
            parent = node_ids[*node.parent];
        }
        json mesh = nullptr;
        if (node.mesh.has_value() && *node.mesh < asset.meshes.size()) {
            mesh = asset.meshes[*node.mesh].id.to_string();
        }
        nodes.push_back({
            {"id", node.id.to_string()},
            {"name", node.name},
            {"path", node.path},
            {"sourceIndex", node.source_index},
            {"parentId", std::move(parent)},
            {"meshId", std::move(mesh)},
            {"translation", {node.translation.x, node.translation.y, node.translation.z}},
            {"rotation", {node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w}},
            {"scale", {node.scale.x, node.scale.y, node.scale.z}},
        });
    }

    json meshes = json::array();
    for (std::size_t mesh_index = 0; mesh_index < asset.meshes.size(); ++mesh_index) {
        const auto& mesh = asset.meshes[mesh_index];
        json primitives = json::array();
        for (std::size_t primitive_index = 0; primitive_index < mesh.primitives.size();
             ++primitive_index) {
            const auto& primitive = mesh.primitives[primitive_index];
            const auto& offsets = primitive_offsets[mesh_index][primitive_index];
            json material = nullptr;
            if (primitive.material.has_value() && *primitive.material < asset.materials.size()) {
                material = asset.materials[*primitive.material].id.to_string();
            }
            primitives.push_back({
                {"id", primitive.id.to_string()},
                {"materialId", std::move(material)},
                {"bounds", bounds_json(primitive.bounds)},
                {"vertexOffset", offsets.vertex_offset},
                {"vertexCount", offsets.vertex_count},
                {"indexOffset", offsets.index_offset},
                {"indexCount", offsets.index_count},
            });
        }
        meshes.push_back({
            {"id", mesh.id.to_string()},
            {"name", mesh.name},
            {"primitives", std::move(primitives)},
        });
    }

    json materials = json::array();
    for (const auto& material : asset.materials) {
        materials.push_back(cooked_material_json(material, asset));
    }

    json texture_array = json::array();
    std::size_t texture_offset = kTextureHeaderSize;
    for (std::size_t index = 0; index < textures.size(); ++index) {
        const auto& texture = textures[index];
        texture_array.push_back({
            {"id", texture.id.to_string()},
            {"textureId", texture.texture_id.to_string()},
            {"name", texture.name},
            {"imageIndex", asset.textures[index].image.value_or(0)},
            {"colorSpace", color_space_name(texture.color_space)},
            {"width", texture.width},
            {"height", texture.height},
            {"offset", texture_offset},
            {"length", texture.rgba8.size()},
            {"sampler", sampler_json(texture.sampler)},
        });
        texture_offset += 12U + texture.rgba8.size();
    }

    return {
        {"formatVersion", 1},
        {"cacheKey", record.cache_key},
        {"sourceHash", record.source_hash},
        {"importerVersion", kGltfImporterVersion},
        {"meshFile", "meshes.bin"},
        {"textureFile", "textures.bin"},
        {"nodes", std::move(nodes)},
        {"meshes", std::move(meshes)},
        {"materials", std::move(materials)},
        {"textures", std::move(texture_array)},
    };
}

[[nodiscard]] Result<void> write_cooked(const std::filesystem::path& cache_dir,
                                        const AssetRecord& record, const ImportedAsset& asset,
                                        bool dry_run = false) {
    std::vector<std::byte> mesh_bytes;
    auto primitive_offsets = write_mesh_buffers(mesh_bytes, asset);
    if (!primitive_offsets) {
        return core::make_failure<void>(primitive_offsets.error());
    }
    auto textures = cook_textures(asset);
    if (!textures) {
        return core::make_failure<void>(textures.error());
    }
    auto texture_bytes = write_texture_buffers(*textures);
    if (!texture_bytes) {
        return core::make_failure<void>(texture_bytes.error());
    }
    if (dry_run) {
        return {};
    }
    if (auto written = write_bytes(cache_dir / "meshes.bin", mesh_bytes); !written) {
        return written;
    }
    if (auto written = write_bytes(cache_dir / "textures.bin", *texture_bytes); !written) {
        return written;
    }
    return write_json(cache_dir / "cooked.json",
                      cooked_document(record, asset, *primitive_offsets, *textures));
}

[[nodiscard]] bool cache_matches(const std::filesystem::path& cache_dir,
                                 std::string_view cache_key) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(cache_dir / "cooked.json", ec) || ec ||
        !std::filesystem::is_regular_file(cache_dir / "meshes.bin", ec) || ec ||
        !std::filesystem::is_regular_file(cache_dir / "textures.bin", ec) || ec) {
        return false;
    }
    auto bytes = read_file(cache_dir / "cooked.json", std::size_t{16} * 1024U * 1024U);
    if (!bytes) {
        return false;
    }
    try {
        const auto document =
            json::parse(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        return document.value("cacheKey", "") == cache_key;
    } catch (const json::exception&) {
        return false;
    }
}

void remove_old_cache_directories(const std::filesystem::path& root, const Uuid& asset_id,
                                  std::string_view keep_path) {
    std::error_code ec;
    const auto directory = cache_root(root, asset_id);
    if (!std::filesystem::is_directory(directory, ec) || ec) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory(ec) && !ec &&
            entry.path() != root / std::filesystem::path(keep_path)) {
            std::filesystem::remove_all(entry.path(), ec);
            ec.clear();
        }
    }
}

[[nodiscard]] Result<void> write_sidecar(const std::filesystem::path& root,
                                         const AssetRecord& record, const ImportedAsset& asset) {
    return write_json(sidecar_path(root, record.asset_id), sidecar_document(record, asset));
}

[[nodiscard]] Result<void> check_project_root(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(root / "project.json", ec) || ec) {
        return core::make_failure<void>(asset_error(
            ErrorCode::NotFound, "project root does not contain project.json",
            "Pass the directory of an existing project.", {{"root", root.generic_string()}}));
    }
    auto sandbox = core::PathSandbox::create(root);
    if (!sandbox) {
        return core::make_failure<void>(sandbox.error());
    }
    return {};
}

[[nodiscard]] std::string default_asset_name(const std::filesystem::path& source) {
    const std::string stem = source.stem().string();
    return stem.empty() ? "Asset" : stem;
}

[[nodiscard]] core::Result<LoadedAsset> load_from_files(const std::filesystem::path& root,
                                                        const AssetRecord& record) {
    auto sidecar_bytes =
        read_file(sidecar_path(root, record.asset_id), std::size_t{16} * 1024U * 1024U);
    if (!sidecar_bytes) {
        return core::make_failure<LoadedAsset>(sidecar_bytes.error());
    }
    auto cache_dir = resolve_inside_project(root, record.cache_path, "cooked.path");
    if (!cache_dir) {
        return core::make_failure<LoadedAsset>(cache_dir.error());
    }
    auto cooked_path =
        resolve_inside_project(root, record.cache_path + "/cooked.json", "cooked.path");
    if (!cooked_path) {
        return core::make_failure<LoadedAsset>(cooked_path.error());
    }
    auto cooked_bytes = read_file(*cooked_path, std::size_t{16} * 1024U * 1024U);
    if (!cooked_bytes) {
        return core::make_failure<LoadedAsset>(cooked_bytes.error());
    }
    auto meshes_path =
        resolve_inside_project(root, record.cache_path + "/meshes.bin", "cooked.path");
    if (!meshes_path) {
        return core::make_failure<LoadedAsset>(meshes_path.error());
    }
    auto mesh_bytes = read_file(*meshes_path, std::size_t{1024} * 1024U * 1024U);
    if (!mesh_bytes) {
        return core::make_failure<LoadedAsset>(mesh_bytes.error());
    }
    auto textures_path =
        resolve_inside_project(root, record.cache_path + "/textures.bin", "cooked.path");
    if (!textures_path) {
        return core::make_failure<LoadedAsset>(textures_path.error());
    }
    auto texture_bytes = read_file(*textures_path, std::size_t{1024} * 1024U * 1024U);
    if (!texture_bytes) {
        return core::make_failure<LoadedAsset>(texture_bytes.error());
    }
    try {
        const auto sidecar = json::parse(std::string(
            reinterpret_cast<const char*>(sidecar_bytes->data()), sidecar_bytes->size()));
        const auto cooked = json::parse(
            std::string(reinterpret_cast<const char*>(cooked_bytes->data()), cooked_bytes->size()));
        nlohmann::json_schema::json_validator validator(json::parse(detail::kCookedSchema));
        validator.validate(cooked);
        if (sidecar.at("importerVersion").get<std::string>() != std::string(kGltfImporterVersion) ||
            cooked.at("importerVersion").get<std::string>() != std::string(kGltfImporterVersion) ||
            cooked.at("sourceHash").get<std::string>() != record.source_hash) {
            return core::make_failure<LoadedAsset>(asset_error(
                ErrorCode::Validation, "asset cache requires reimport with the current importer"));
        }
        if (cooked.value("cacheKey", "") != record.cache_key) {
            return core::make_failure<LoadedAsset>(asset_error(
                ErrorCode::Validation, "cooked cache key does not match the asset sidecar",
                "Reimport the asset to rebuild the cooked cache."));
        }
        const auto mesh_data = std::span<const std::byte>(mesh_bytes->data(), mesh_bytes->size());
        if (read_u32(mesh_data, 0U) != kMeshMagic || read_u32(mesh_data, 4U) != kCookedVersion) {
            return core::make_failure<LoadedAsset>(
                asset_error(ErrorCode::Validation, "cooked mesh file has an invalid header"));
        }
        const auto texture_data =
            std::span<const std::byte>(texture_bytes->data(), texture_bytes->size());
        if (read_u32(texture_data, 0U) != kTextureMagic ||
            read_u32(texture_data, 4U) != kCookedVersion) {
            return core::make_failure<LoadedAsset>(
                asset_error(ErrorCode::Validation, "cooked texture file has an invalid header"));
        }

        LoadedAsset loaded;
        loaded.record = record;
        auto& asset = loaded.asset;
        asset.source = record.source;
        asset.animation_channel_count = sidecar.value("animationChannelCount", 0U);

        std::unordered_map<std::string, std::size_t> texture_ids;
        const auto& texture_json = cooked.at("textures");
        asset.textures.reserve(texture_json.size());
        loaded.textures.reserve(texture_json.size());
        for (std::size_t index = 0; index < texture_json.size(); ++index) {
            const auto& item = texture_json[index];
            Texture texture;
            texture.id = parse_uuid(item, "id");
            texture.name = item.value("name", "");
            texture.color_space = parse_color_space(item.at("colorSpace"));
            texture.image = std::nullopt;
            Sampler sampler = parse_sampler(item.value("sampler", json::object()));
            asset.samplers.push_back(sampler);
            texture.sampler = asset.samplers.size() - 1U;
            texture_ids.emplace(texture.id.to_string(), index);
            asset.textures.push_back(std::move(texture));

            CookedTexture cooked_texture;
            cooked_texture.id = parse_uuid(item, "id");
            cooked_texture.texture_id = cooked_texture.id;
            cooked_texture.name = item.value("name", "");
            cooked_texture.color_space = parse_color_space(item.at("colorSpace"));
            cooked_texture.width = item.at("width").get<std::uint32_t>();
            cooked_texture.height = item.at("height").get<std::uint32_t>();
            const auto offset = item.at("offset").get<std::size_t>();
            const auto length = item.at("length").get<std::size_t>();
            if (offset > texture_data.size() || length > texture_data.size() - offset ||
                length > std::numeric_limits<std::size_t>::max() - 12U ||
                offset + 12U + length > texture_data.size()) {
                return core::make_failure<LoadedAsset>(
                    asset_error(ErrorCode::Validation, "cooked texture offset is out of range"));
            }
            const auto width = read_u32(texture_data, offset);
            const auto height = read_u32(texture_data, offset + 4U);
            const auto stored_length = read_u32(texture_data, offset + 8U);
            if (width != cooked_texture.width || height != cooked_texture.height ||
                stored_length != length ||
                length != static_cast<std::size_t>(width) * height * 4U) {
                return core::make_failure<LoadedAsset>(
                    asset_error(ErrorCode::Validation, "cooked texture metadata is inconsistent"));
            }
            cooked_texture.rgba8.resize(length);
            std::memcpy(cooked_texture.rgba8.data(), texture_data.data() + offset + 12U, length);
            cooked_texture.sampler = sampler;
            loaded.textures.push_back(std::move(cooked_texture));
        }

        std::unordered_map<std::string, std::size_t> material_ids;
        const auto& material_json = cooked.at("materials");
        asset.materials.reserve(material_json.size());
        for (std::size_t index = 0; index < material_json.size(); ++index) {
            const auto& item = material_json[index];
            Material material;
            material.id = parse_uuid(item, "id");
            material.name = item.value("name", "");
            const auto& pbr = item.at("pbr");
            material.base_color_factor = pbr.at("baseColorFactor").get<std::array<float, 4>>();
            material.metallic_factor = pbr.at("metallicFactor").get<float>();
            material.roughness_factor = pbr.at("roughnessFactor").get<float>();
            material.emissive_factor = pbr.at("emissiveFactor").get<std::array<float, 3>>();
            material.alpha_mode =
                pbr.value("alphaMode", "OPAQUE") == "MASK" ? AlphaMode::Mask : AlphaMode::Opaque;
            material.alpha_cutoff = pbr.value("alphaCutoff", 0.5F);
            material.double_sided = pbr.value("doubleSided", false);
            material.normal_scale = item.value("normalScale", 1.0F);
            material.occlusion_strength = item.value("occlusionStrength", 1.0F);
            const auto parse_reference =
                [&texture_ids](const json& value) -> std::optional<TextureReference> {
                if (value.is_null()) {
                    return std::nullopt;
                }
                const auto found = texture_ids.find(value.get<std::string>());
                if (found == texture_ids.end()) {
                    return std::nullopt;
                }
                return TextureReference{found->second, 0};
            };
            const auto& textures = pbr.at("textures");
            material.base_color_texture = parse_reference(textures.value("baseColor", json()));
            material.metallic_roughness_texture =
                parse_reference(textures.value("metallicRoughness", json()));
            material.normal_texture = parse_reference(textures.value("normal", json()));
            material.occlusion_texture = parse_reference(textures.value("occlusion", json()));
            material.emissive_texture = parse_reference(textures.value("emissive", json()));
            material_ids.emplace(material.id.to_string(), index);
            asset.materials.push_back(std::move(material));
        }

        std::unordered_map<std::string, std::size_t> mesh_ids;
        const auto& mesh_json = cooked.at("meshes");
        asset.meshes.reserve(mesh_json.size());
        for (std::size_t mesh_index = 0; mesh_index < mesh_json.size(); ++mesh_index) {
            const auto& item = mesh_json[mesh_index];
            Mesh mesh;
            mesh.id = parse_uuid(item, "id");
            mesh.name = item.value("name", "");
            const auto& primitives = item.at("primitives");
            mesh.primitives.reserve(primitives.size());
            for (const auto& primitive_json : primitives) {
                Primitive primitive;
                primitive.id = parse_uuid(primitive_json, "id");
                const auto vertex_offset = primitive_json.at("vertexOffset").get<std::size_t>();
                const auto vertex_count = primitive_json.at("vertexCount").get<std::size_t>();
                const auto index_offset = primitive_json.at("indexOffset").get<std::size_t>();
                const auto index_count = primitive_json.at("indexCount").get<std::size_t>();
                if (vertex_count >
                        std::numeric_limits<std::size_t>::max() / (12U * sizeof(float)) ||
                    vertex_offset > mesh_data.size() ||
                    vertex_count * 12U * sizeof(float) > mesh_data.size() - vertex_offset ||
                    index_count > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t) ||
                    index_offset > mesh_data.size() ||
                    index_count * sizeof(std::uint32_t) > mesh_data.size() - index_offset) {
                    return core::make_failure<LoadedAsset>(
                        asset_error(ErrorCode::Validation, "cooked mesh offset is out of range"));
                }
                primitive.vertices.resize(vertex_count);
                for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
                    const std::size_t base = vertex_offset + (vertex * 12U * sizeof(float));
                    auto& output = primitive.vertices[vertex];
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        output.position[axis] = read_float(mesh_data, base + (axis * 4U));
                    }
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        output.normal[axis] = read_float(mesh_data, base + 12U + (axis * 4U));
                    }
                    for (std::size_t axis = 0; axis < 4; ++axis) {
                        output.tangent[axis] = read_float(mesh_data, base + 24U + (axis * 4U));
                    }
                    for (std::size_t axis = 0; axis < 2; ++axis) {
                        output.uv0[axis] = read_float(mesh_data, base + 40U + (axis * 4U));
                    }
                }
                primitive.indices.resize(index_count);
                for (std::size_t index = 0; index < index_count; ++index) {
                    primitive.indices[index] =
                        read_u32(mesh_data, index_offset + (index * sizeof(std::uint32_t)));
                }
                const auto& bounds = primitive_json.at("bounds");
                primitive.bounds.min = bounds.at("min").get<std::array<float, 3>>();
                primitive.bounds.max = bounds.at("max").get<std::array<float, 3>>();
                const auto material = primitive_json.value("materialId", json());
                if (!material.is_null()) {
                    const auto found = material_ids.find(material.get<std::string>());
                    if (found == material_ids.end()) {
                        return core::make_failure<LoadedAsset>(asset_error(
                            ErrorCode::Validation, "cooked primitive material is missing"));
                    }
                    primitive.material = found->second;
                }
                mesh.primitives.push_back(std::move(primitive));
            }
            mesh_ids.emplace(mesh.id.to_string(), mesh_index);
            asset.meshes.push_back(std::move(mesh));
        }

        const auto& node_json = cooked.at("nodes");
        std::unordered_map<std::string, std::size_t> node_ids;
        for (std::size_t index = 0; index < node_json.size(); ++index) {
            node_ids.emplace(parse_uuid(node_json[index], "id").to_string(), index);
        }
        asset.nodes.reserve(node_json.size());
        for (const auto& item : node_json) {
            Node node;
            node.id = parse_uuid(item, "id");
            node.name = item.value("name", "");
            node.path = item.value("path", node.name);
            node.source_index = item.value("sourceIndex", 0U);
            const auto translation = item.at("translation").get<std::array<double, 3>>();
            node.translation = {translation[0], translation[1], translation[2]};
            const auto rotation = item.at("rotation").get<std::array<float, 4>>();
            node.rotation = {rotation[0], rotation[1], rotation[2], rotation[3]};
            const auto scale = item.at("scale").get<std::array<float, 3>>();
            node.scale = {scale[0], scale[1], scale[2]};
            const auto parent = item.value("parentId", json());
            if (!parent.is_null()) {
                const auto found = node_ids.find(parent.get<std::string>());
                if (found == node_ids.end()) {
                    return core::make_failure<LoadedAsset>(
                        asset_error(ErrorCode::Validation, "cooked node parent is missing"));
                }
                node.parent = found->second;
            }
            const auto mesh = item.value("meshId", json());
            if (!mesh.is_null()) {
                const auto found = mesh_ids.find(mesh.get<std::string>());
                if (found == mesh_ids.end()) {
                    return core::make_failure<LoadedAsset>(
                        asset_error(ErrorCode::Validation, "cooked node mesh is missing"));
                }
                node.mesh = found->second;
            }
            asset.nodes.push_back(std::move(node));
        }
        for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
            const auto parent = asset.nodes[index].parent;
            if (parent.has_value()) {
                asset.nodes[*parent].children.push_back(index);
            }
        }
        const auto& roots = sidecar.at("metadata").at("sceneRoots");
        if (!roots.is_array()) {
            return core::make_failure<LoadedAsset>(
                asset_error(ErrorCode::Validation, "invalid asset scene roots"));
        }
        for (const auto& root_id : roots) {
            const auto found = node_ids.find(root_id.get<std::string>());
            if (found == node_ids.end() || asset.nodes[found->second].parent.has_value()) {
                return core::make_failure<LoadedAsset>(
                    asset_error(ErrorCode::Validation, "invalid asset scene root reference"));
            }
            asset.scene_roots.push_back(found->second);
        }
        return loaded;
    } catch (const std::exception& error) {
        return core::make_failure<LoadedAsset>(
            asset_error(ErrorCode::Validation, "cooked asset metadata is invalid", error.what()));
    }
}

} // namespace

core::Result<ImportResult> import_asset(const std::filesystem::path& project_root,
                                        const std::filesystem::path& source, std::string name,
                                        bool dry_run) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<ImportResult>(valid.error());
    }
    auto bytes = read_file(source, kMaxSourceBytes);
    if (!bytes) {
        return core::make_failure<ImportResult>(bytes.error());
    }
    auto imported = import_glb(*bytes, source.generic_string());
    if (!imported) {
        return core::make_failure<ImportResult>(imported.error());
    }
    const auto source_hash = core::sha256_hex(*bytes);
    ImportResult result;
    result.record.asset_id = Uuid::generate();
    result.record.name = name.empty() ? default_asset_name(source) : std::move(name);
    result.record.source = stored_source(project_root, source);
    result.record.source_copy = "assets/" + result.record.asset_id.to_string() + "/source.glb";
    result.record.source_hash = source_hash;
    result.record.cache_key =
        core::sha256_hex(source_hash + "|" + std::string(kGltfImporterVersion) + "|{}");
    // The directory uses a short key prefix to stay below the Windows path
    // limit; the full SHA-256 key remains in the sidecar and cooked manifest.
    result.record.cache_path = ".engine/cache/" + result.record.asset_id.to_string() + "/" +
                               result.record.cache_key.substr(0U, 16U);
    result.record.animation_channel_count = imported->animation_channel_count;

    assign_new_ids(*imported);
    if (auto colors = assign_texture_color_spaces(*imported); !colors) {
        return core::make_failure<ImportResult>(colors.error());
    }
    if (auto tangents = generate_missing_tangents(*imported); !tangents) {
        return core::make_failure<ImportResult>(tangents.error());
    }
    auto source_copy =
        resolve_inside_project(project_root, result.record.source_copy, "sourceCopy");
    auto cache_dir = resolve_inside_project(project_root, result.record.cache_path, "cooked.path");
    if (!source_copy) {
        return core::make_failure<ImportResult>(source_copy.error());
    }
    if (!cache_dir) {
        return core::make_failure<ImportResult>(cache_dir.error());
    }
    if (auto cooked = write_cooked(*cache_dir, result.record, *imported, dry_run); !cooked) {
        return core::make_failure<ImportResult>(cooked.error());
    }
    if (dry_run) {
        return result;
    }
    if (auto copied = write_bytes(*source_copy, *bytes); !copied) {
        return core::make_failure<ImportResult>(copied.error());
    }
    if (auto written = write_sidecar(project_root, result.record, *imported); !written) {
        return core::make_failure<ImportResult>(written.error());
    }
    return result;
}

core::Result<ImportResult> reimport_asset(const std::filesystem::path& project_root,
                                          const core::Uuid& asset_id, bool dry_run) {
    return reimport_asset(project_root, asset_id, dry_run, false);
}

core::Result<ImportResult> reimport_asset(const std::filesystem::path& project_root,
                                          const core::Uuid& asset_id, bool dry_run,
                                          bool project_sources_only) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<ImportResult>(valid.error());
    }
    auto sidecar = read_sidecar_json(project_root, asset_id);
    if (!sidecar) {
        return core::make_failure<ImportResult>(sidecar.error());
    }
    auto record = record_from_sidecar(*sidecar, asset_id);
    if (!record) {
        return core::make_failure<ImportResult>(record.error());
    }
    std::filesystem::path source;
    if (project_sources_only) {
        auto original = resolve_inside_project(project_root, record->source, "source");
        std::error_code ec;
        if (original && std::filesystem::is_regular_file(*original, ec) && !ec) {
            source = *original;
        } else {
            auto copy = resolve_inside_project(project_root, record->source_copy, "sourceCopy");
            if (!copy) {
                return core::make_failure<ImportResult>(copy.error());
            }
            source = *copy;
        }
    } else {
        source = resolve_source(project_root, record->source, record->source_copy);
    }
    if (source.empty()) {
        return core::make_failure<ImportResult>(
            asset_error(ErrorCode::NotFound, "asset source and source copy are both missing",
                        "Restore the original source or the project-relative source copy.",
                        {{"source", record->source}, {"sourceCopy", record->source_copy}}));
    }
    auto bytes = read_file(source, kMaxSourceBytes);
    if (!bytes) {
        return core::make_failure<ImportResult>(bytes.error());
    }
    auto imported = import_glb(*bytes, source.generic_string());
    if (!imported) {
        return core::make_failure<ImportResult>(imported.error());
    }
    record->source_hash = core::sha256_hex(*bytes);
    record->cache_key =
        core::sha256_hex(record->source_hash + "|" + std::string(kGltfImporterVersion) + "|{}");
    record->cache_path =
        ".engine/cache/" + asset_id.to_string() + "/" + record->cache_key.substr(0U, 16U);
    record->animation_channel_count = imported->animation_channel_count;

    assign_existing_ids(*imported, *sidecar);
    if (auto colors = assign_texture_color_spaces(*imported); !colors) {
        return core::make_failure<ImportResult>(colors.error());
    }
    if (auto tangents = generate_missing_tangents(*imported); !tangents) {
        return core::make_failure<ImportResult>(tangents.error());
    }
    auto source_copy = resolve_inside_project(project_root, record->source_copy, "sourceCopy");
    if (!source_copy) {
        return core::make_failure<ImportResult>(source_copy.error());
    }
    ImportResult result;
    result.record = *record;
    result.cache_hit = cache_matches(project_root / record->cache_path, record->cache_key) &&
                       load_from_files(project_root, *record).has_value();
    auto cache_dir = resolve_inside_project(project_root, record->cache_path, "cooked.path");
    if (!cache_dir) {
        return core::make_failure<ImportResult>(cache_dir.error());
    }
    if (dry_run || !result.cache_hit) {
        if (auto cooked = write_cooked(*cache_dir, *record, *imported, dry_run); !cooked) {
            return core::make_failure<ImportResult>(cooked.error());
        }
        if (!dry_run) {
            remove_old_cache_directories(project_root, asset_id, record->cache_path);
        }
    }
    if (dry_run) {
        return result;
    }
    if (auto copied = write_bytes(*source_copy, *bytes); !copied) {
        return core::make_failure<ImportResult>(copied.error());
    }
    if (auto written = write_sidecar(project_root, *record, *imported); !written) {
        return core::make_failure<ImportResult>(written.error());
    }
    return result;
}

core::Result<LoadedAsset> load_asset(const std::filesystem::path& project_root,
                                     const core::Uuid& asset_id) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<LoadedAsset>(valid.error());
    }
    auto sidecar = read_sidecar_json(project_root, asset_id);
    if (!sidecar) {
        return core::make_failure<LoadedAsset>(sidecar.error());
    }
    auto record = record_from_sidecar(*sidecar, asset_id);
    if (!record) {
        return core::make_failure<LoadedAsset>(record.error());
    }
    return load_from_files(project_root, *record);
}

core::Result<nlohmann::json> inspect_asset(const std::filesystem::path& project_root,
                                           const core::Uuid& asset_id) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<json>(valid.error());
    }
    return read_sidecar_json(project_root, asset_id);
}

core::Result<AssetRecord> read_asset_record(const std::filesystem::path& project_root,
                                            const core::Uuid& asset_id) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<AssetRecord>(valid.error());
    }
    auto sidecar = read_sidecar_json(project_root, asset_id);
    if (!sidecar) {
        return core::make_failure<AssetRecord>(sidecar.error());
    }
    return record_from_sidecar(*sidecar, asset_id);
}

core::Result<std::vector<AssetRecord>> list_assets(const std::filesystem::path& project_root) {
    if (auto valid = check_project_root(project_root); !valid) {
        return core::make_failure<std::vector<AssetRecord>>(valid.error());
    }
    std::vector<AssetRecord> records;
    std::error_code ec;
    auto resolved = resolve_inside_project(project_root, "assets", "assets");
    if (!resolved) {
        return core::make_failure<std::vector<AssetRecord>>(resolved.error());
    }
    const auto assets = *resolved;
    if (!std::filesystem::is_directory(assets, ec) || ec) {
        return records;
    }
    for (const auto& entry : std::filesystem::directory_iterator(assets, ec)) {
        if (ec) {
            return core::make_failure<std::vector<AssetRecord>>(
                asset_error(ErrorCode::Io, "cannot list the assets directory", ec.message()));
        }
        if (!entry.is_directory()) {
            continue;
        }
        const auto parsed = Uuid::parse(entry.path().filename().string());
        if (!parsed) {
            continue;
        }
        auto record = read_asset_record(project_root, *parsed);
        if (!record) {
            return core::make_failure<std::vector<AssetRecord>>(record.error());
        }
        records.push_back(std::move(*record));
    }
    std::ranges::sort(records, [](const AssetRecord& left, const AssetRecord& right) {
        return left.asset_id < right.asset_id;
    });
    return records;
}

} // namespace engine::asset
