#pragma once

#include <engine/asset/gltf_import.hpp>

#include <cstdint>

namespace engine::asset {

inline constexpr std::uint64_t kMaxTangentWeldPairs = 1000000U;

// Runs upstream MikkTSpace for normal-mapped primitives without source tangents.
// Rebuilds the index list, splitting vertices at tangent-space discontinuities.
// Preflights the entire asset's dense-position work budget before any mutation.
[[nodiscard]] core::Result<void> generate_missing_tangents(ImportedAsset& asset);

} // namespace engine::asset
