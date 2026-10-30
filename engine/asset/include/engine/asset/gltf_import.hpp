#pragma once

#include <engine/core/error.hpp>
#include <engine/core/math.hpp>
#include <engine/core/uuid.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine::asset {

// Bumped whenever the in-memory interpretation of a source asset changes.
inline constexpr std::string_view kGltfImporterVersion = "gltf-import@3";

inline constexpr std::size_t kMaxSourceBytes = std::size_t{512} * 1024U * 1024U;
inline constexpr std::size_t kMaxNodes = 65536U;
inline constexpr std::size_t kMaxMeshes = 65536U;
inline constexpr std::size_t kMaxPrimitives = 262144U;
inline constexpr std::size_t kMaxVerticesPerPrimitive = std::size_t{4} * 1024U * 1024U;
inline constexpr std::size_t kMaxIndicesPerPrimitive = std::size_t{16} * 1024U * 1024U;
inline constexpr std::size_t kMaxImageBytes = std::size_t{64} * 1024U * 1024U;
inline constexpr std::size_t kMaxNodeDepth = 1024U;

struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{0.0F, 0.0F, 1.0F};
    std::array<float, 4> tangent{0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 2> uv0{};
};

struct Bounds {
    std::array<float, 3> min{};
    std::array<float, 3> max{};
};

struct Primitive {
    core::Uuid id;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::optional<std::size_t> material;
    Bounds bounds;
};

struct Mesh {
    core::Uuid id;
    std::string name;
    std::vector<Primitive> primitives;
};

enum class AlphaMode : std::uint8_t {
    Opaque,
    Mask,
};

enum class Filter : std::uint16_t {
    Nearest = 9728,
    Linear = 9729,
    NearestMipmapNearest = 9984,
    LinearMipmapNearest = 9985,
    NearestMipmapLinear = 9986,
    LinearMipmapLinear = 9987,
};

enum class Wrap : std::uint16_t {
    ClampToEdge = 33071,
    MirroredRepeat = 33648,
    Repeat = 10497,
};

struct TextureReference {
    std::size_t texture{0};
    std::uint32_t tex_coord{0};
};

struct Material {
    core::Uuid id;
    std::string name;
    std::array<float, 4> base_color_factor{1.0F, 1.0F, 1.0F, 1.0F};
    float metallic_factor{1.0F};
    float roughness_factor{1.0F};
    std::array<float, 3> emissive_factor{0.0F, 0.0F, 0.0F};
    AlphaMode alpha_mode{AlphaMode::Opaque};
    float alpha_cutoff{0.5F};
    bool double_sided{false};
    std::optional<TextureReference> base_color_texture;
    std::optional<TextureReference> metallic_roughness_texture;
    std::optional<TextureReference> normal_texture;
    float normal_scale{1.0F};
    std::optional<TextureReference> occlusion_texture;
    float occlusion_strength{1.0F};
    std::optional<TextureReference> emissive_texture;
};

enum class ImageMimeType : std::uint8_t {
    Png,
    Jpeg,
};

enum class TextureColorSpace : std::uint8_t {
    Srgb,
    Linear,
};

struct Image {
    std::string name;
    ImageMimeType mime_type{ImageMimeType::Png};
    // Encoded PNG/JPEG bytes. Decoding happens during cooking, not import.
    std::vector<std::byte> bytes;
};

struct Sampler {
    Filter mag_filter{Filter::Linear};
    Filter min_filter{Filter::LinearMipmapLinear};
    Wrap wrap_s{Wrap::Repeat};
    Wrap wrap_t{Wrap::Repeat};
};

struct Texture {
    core::Uuid id;
    std::string name;
    TextureColorSpace color_space{TextureColorSpace::Linear};
    std::optional<std::size_t> image;
    std::optional<std::size_t> sampler;
};

struct Node {
    core::Uuid id;
    std::string name;
    // Stable reimport key derived from the node path; source_index is the
    // fallback when names change or collide.
    std::string path;
    std::size_t source_index{0};
    std::optional<std::size_t> parent;
    std::vector<std::size_t> children;
    std::optional<std::size_t> mesh;
    core::Vec3d translation;
    core::Quat rotation;
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct WorldTransform {
    core::Vec3d position;
    core::Quat rotation;
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct ImportedAsset {
    std::string source;
    std::string generator;
    std::optional<std::size_t> default_scene;
    std::string scene_name;
    std::vector<std::size_t> scene_roots;
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Image> images;
    std::vector<Sampler> samplers;
    std::size_t animation_channel_count{0};
};

// Imports a self-contained GLB. External buffers, external images and glTF 2.0
// JSON containers are rejected with stable error codes.
[[nodiscard]] core::Result<ImportedAsset> import_glb(std::span<const std::byte> bytes,
                                                     std::string source_name);
[[nodiscard]] core::Result<ImportedAsset> import_glb_file(const std::filesystem::path& path);

// Composes local node transforms into world space. Translation is composed in
// double precision; rotation and scale are converted to float at the boundary.
[[nodiscard]] std::vector<WorldTransform> world_transforms(const ImportedAsset& asset);

} // namespace engine::asset
