#pragma once

#include <engine/asset/asset_library.hpp>
#include <engine/scene/scene_view.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <utility>

namespace engine::cli {

// Resolve only referenced project assets, once per stable UUID. PBR resolves
// their node hierarchies read-only and shares uploaded resources across instances.
// Loading is read-only: missing/stale caches must be repaired by asset reimport.
template <class Renderer>
[[nodiscard]] core::Result<void> upload_scene_assets(Renderer& renderer,
                                                     const std::filesystem::path& root,
                                                     const scene::SceneView& view) {
    std::set<std::string> uploaded;
    for (const auto entity : view.entities()) {
        if (!view.scene().has<scene::AssetInstance>(entity)) {
            continue;
        }
        const auto& reference = view.scene().component<scene::AssetInstance>(entity).asset_id;
        if (!uploaded.insert(reference).second) {
            continue;
        }
        const auto id = core::Uuid::parse(reference);
        if (!id) {
            return core::make_failure<void>(id.error());
        }
        auto loaded = asset::load_asset(root, *id);
        if (!loaded) {
            auto error = loaded.error();
            error.details["assetId"] = reference;
            return core::make_failure<void>(std::move(error));
        }
        auto result = renderer.upload_asset(*loaded);
        if (!result) {
            return result;
        }
    }
    return {};
}

} // namespace engine::cli
