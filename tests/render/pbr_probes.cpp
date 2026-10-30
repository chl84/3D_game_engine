#include "pbr_probes.hpp"
#include <engine/render/offscreen_renderer.hpp>
#include <iostream>
#include <stdexcept>

namespace engine::test {
namespace {
using namespace engine;
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
template <class T> T take(core::Result<T>&& value) {
    if (!value) {
        throw std::runtime_error(value.error().message + " " + value.error().details.dump());
    }
    return std::move(*value);
}
void good(core::Result<void>&& value) {
    if (!value) {
        throw std::runtime_error(value.error().message);
    }
}
asset::LoadedAsset quad() {
    asset::LoadedAsset a;
    a.record.asset_id = core::Uuid::generate();
    asset::Primitive p;
    p.material = 0;
    p.indices = {0, 1, 2, 0, 2, 3};
    for (const auto& xy : std::array<std::array<float, 2>, 4>{
             {{-0.6F, -0.6F}, {0.6F, -0.6F}, {0.6F, 0.6F}, {-0.6F, 0.6F}},
         }) {
        asset::Vertex v;
        v.position = {xy[0], xy[1], 0};
        v.normal = {0, 0, 1};
        v.tangent = {1, 0, 0, 1};
        v.uv0 = {(xy[0] + 0.6F) / 1.2F, (xy[1] + 0.6F) / 1.2F};
        p.vertices.push_back(v);
    }
    asset::Mesh m;
    m.primitives.push_back(p);
    a.asset.meshes.push_back(m);
    asset::Node n;
    n.mesh = 0;
    a.asset.nodes.push_back(n);
    a.asset.scene_roots = {0};
    asset::Material material;
    material.metallic_factor = 0;
    material.roughness_factor = 1;
    material.double_sided = true;
    a.asset.materials.push_back(material);
    return a;
}
void add_texture(asset::LoadedAsset& a, asset::TextureColorSpace space,
                 std::array<std::uint8_t, 4> pixel) {
    asset::Texture source;
    source.id = core::Uuid::generate();
    source.color_space = space;
    a.asset.textures.push_back(source);
    asset::CookedTexture t;
    t.id = source.id;
    t.texture_id = source.id;
    t.width = 1;
    t.height = 1;
    t.color_space = space;
    t.rgba8.assign(pixel.begin(), pixel.end());
    a.textures.push_back(t);
}
scene::Scene make_scene(const std::string& id) {
    scene::Scene s;
    auto camera = take(s.create_entity("Camera"));
    good(s.add_component<scene::Camera>(camera));
    s.transform(camera).position = {0, 0, 3};
    auto instance = take(s.create_entity("Probe"));
    good(s.add_component<scene::AssetInstance>(instance));
    s.component<scene::AssetInstance>(instance).asset_id = id;
    return s;
}
int red(render::OffscreenRenderer& renderer, asset::LoadedAsset& a) {
    good(renderer.upload_asset(a));
    auto s = make_scene(a.record.asset_id.to_string());
    auto pixels = take(renderer.render(scene::SceneView(s), 1));
    const int value = pixels[((std::size_t{32} * 64U) + 32U) * 4U];
    good(renderer.retire_asset(a.record.asset_id.to_string()));
    good(renderer.collect_assets());
    return value;
}
void near(int actual, int expected, const char* message) {
    if (std::abs(actual - expected) > 2) {
        std::cerr << message << ": actual=" << actual << " expected=" << expected << '\n';
        throw std::runtime_error(message);
    }
}
} // namespace
void verify_pbr_probes(const render::Device& device) {
    auto renderer = take(render::OffscreenRenderer::create(device, {64, 64}));
    auto base = quad();
    add_texture(base, asset::TextureColorSpace::Srgb, {128, 128, 128, 255});
    base.asset.materials[0].base_color_texture = asset::TextureReference{};
    near(red(renderer, base), 19, "baseColor sRGB decode failed");
    auto linear = base;
    linear.record.asset_id = core::Uuid::generate();
    linear.textures[0].color_space = asset::TextureColorSpace::Linear;
    near(red(renderer, linear), 33, "linear format control failed");
    auto ao = quad();
    add_texture(ao, asset::TextureColorSpace::Linear, {128, 128, 128, 255});
    ao.asset.materials[0].occlusion_texture = asset::TextureReference{};
    near(red(renderer, ao), 33, "occlusion data was treated as sRGB");
    auto mr = quad();
    add_texture(mr, asset::TextureColorSpace::Linear, {0, 200, 128, 255});
    mr.asset.materials[0].metallic_factor = 1;
    mr.asset.materials[0].metallic_roughness_texture = asset::TextureReference{};
    near(red(renderer, mr), 33, "metallic-roughness blue channel or linear data failed");
    auto emissive = quad();
    add_texture(emissive, asset::TextureColorSpace::Linear, {128, 128, 128, 255});
    auto& em = emissive.asset.materials[0];
    em.base_color_factor = {0, 0, 0, 1};
    em.emissive_factor = {1, 1, 1};
    em.emissive_texture = asset::TextureReference{};
    near(red(renderer, emissive), 156, "linear emissive/tone-map pipeline failed");
    auto alpha = quad();
    alpha.asset.materials[0].base_color_factor = {1, 1, 1, 0.25F};
    alpha.asset.materials[0].alpha_mode = asset::AlphaMode::Mask;
    near(red(renderer, alpha), 10, "alpha mask did not discard");
    alpha.record.asset_id = core::Uuid::generate();
    alpha.asset.materials[0].alpha_mode = asset::AlphaMode::Opaque;
    near(red(renderer, alpha), 48, "opaque material incorrectly used alpha");
    auto mip = quad();
    add_texture(mip, asset::TextureColorSpace::Srgb, {0, 0, 0, 255});
    mip.asset.materials[0].base_color_texture = asset::TextureReference{};
    auto& t = mip.textures[0];
    t.width = 8;
    t.height = 8;
    t.rgba8.resize(std::size_t{8} * 8 * 4);
    t.sampler.min_filter = asset::Filter::NearestMipmapNearest;
    t.sampler.mag_filter = asset::Filter::Nearest;
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            auto i = ((y * 8) + x) * 4;
            for (std::size_t c = 0; c < 3; ++c) {
                t.rgba8[i + c] = (x + y) % 2 ? 255 : 0;
            }
            t.rgba8[i + 3] = 255;
        }
    }
    for (auto& v : mip.asset.meshes[0].primitives[0].vertices) {
        v.uv0[0] *= 128;
        v.uv0[1] *= 128;
    }
    near(red(renderer, mip), 33, "sRGB mip generation or LOD sampling failed");
    // Fill both hard caps. Identical images share backing memory, but every
    // image descriptor is initialized and the draw accesses the very last slot.
    auto full = quad();
    add_texture(full, asset::TextureColorSpace::Srgb, {128, 128, 128, 255});
    full.asset.materials[0].base_color_texture = asset::TextureReference{4095, 0};
    auto pixel = full.textures[0];
    full.textures.clear();
    full.asset.textures.clear();
    core::UuidGenerator ids(73);
    for (std::size_t i = 0; i < 4096; ++i) {
        auto copy = pixel;
        copy.id = ids.next();
        copy.texture_id = copy.id;
        asset::Texture source;
        source.id = copy.id;
        source.color_space = copy.color_space;
        full.textures.push_back(copy);
        full.asset.textures.push_back(source);
    }
    full.asset.materials.resize(4096, full.asset.materials[0]);
    full.asset.meshes[0].primitives[0].material = 4095;
    good(renderer.upload_asset(full));
    require(renderer.pbr_stats().textures == 4096 && renderer.pbr_stats().materials == 4096,
            "full bindless slot allocation failed");
    auto scene = make_scene(full.record.asset_id.to_string());
    auto pixels = take(renderer.render(scene::SceneView(scene), 1));
    near(pixels[((std::size_t{32} * 64U) + 32U) * 4U], 19,
         "last bindless image/material slot failed");
    auto extra = quad();
    auto denied = renderer.upload_asset(extra);
    require(!denied && denied.error().code == core::ErrorCode::Validation,
            "full bindless pool accepted another asset");
    good(renderer.retire_asset(full.record.asset_id.to_string()));
    require(renderer.pbr_stats().textures == 4096, "retired slots freed before safe collection");
    good(renderer.collect_assets());
    require(renderer.pbr_stats().textures == 0 && renderer.pbr_stats().materials == 0,
            "bindless slots leaked");
    std::cout << "PBR color-space, alpha, mip and full 4096-slot probes passed\n";
}
} // namespace engine::test
