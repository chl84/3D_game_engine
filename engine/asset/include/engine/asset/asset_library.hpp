#pragma once

#include <engine/asset/gltf_import.hpp>
#include <engine/core/error.hpp>
#include <engine/core/uuid.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace engine::asset {

inline constexpr std::string_view kAssetFormatVersion = "asset@1";
inline constexpr std::string_view kCookedFormatVersion = "engine.asset.cooked@1";

struct AssetRecord {
    core::Uuid asset_id;
    std::string name;
    std::string source;
    std::string source_copy;
    std::string source_hash;
    std::string cache_key;
    std::string cache_path;
    std::size_t animation_channel_count{0};
};

struct ImportResult {
    AssetRecord record;
    bool cache_hit{false};
};

struct CookedTexture {
    core::Uuid id;
    core::Uuid texture_id;
    std::string name;
    TextureColorSpace color_space{TextureColorSpace::Linear};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8;
    Sampler sampler;
};

struct LoadedAsset {
    AssetRecord record;
    ImportedAsset asset;
    std::vector<CookedTexture> textures;
};

// Imports a GLB into assets/<assetId>/source.glb and writes an asset@1 sidecar
// and an engine.asset.cooked@1 cache entry. The first import generates the asset
// UUID and stable node/mesh/primitive/material/texture IDs.
[[nodiscard]] core::Result<ImportResult> import_asset(const std::filesystem::path& project_root,
                                                      const std::filesystem::path& source,
                                                      std::string name = {});

// Reimports from the recorded source (or the project-relative source copy when
// the original is gone) and preserves the asset UUID and stable IDs.
[[nodiscard]] core::Result<ImportResult> reimport_asset(const std::filesystem::path& project_root,
                                                        const core::Uuid& asset_id);

[[nodiscard]] core::Result<LoadedAsset> load_asset(const std::filesystem::path& project_root,
                                                   const core::Uuid& asset_id);

[[nodiscard]] core::Result<AssetRecord> read_asset_record(const std::filesystem::path& project_root,
                                                          const core::Uuid& asset_id);

[[nodiscard]] core::Result<std::vector<AssetRecord>>
list_assets(const std::filesystem::path& project_root);

} // namespace engine::asset
