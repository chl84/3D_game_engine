#pragma once

#include <engine/asset/gltf_import.hpp>

namespace engine::asset {

// Runs upstream MikkTSpace for normal-mapped primitives without source tangents.
// Rebuilds the index list, splitting vertices at tangent-space discontinuities.
[[nodiscard]] core::Result<void> generate_missing_tangents(ImportedAsset& asset);

} // namespace engine::asset
