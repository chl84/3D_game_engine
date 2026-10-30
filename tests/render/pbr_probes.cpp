#include "pbr_probes.hpp"
#include <cmath>
#include <engine/render/offscreen_renderer.hpp>
#include <iostream>
#include <numbers>
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
// Known normal (approximately 0,0.6,0.8) and L=(0,1,1)/sqrt(2).
// Reflection in X preserves its positive Y component only when tangent.w
// changes sign. GGX at roughness=1 gives about 137, versus 74 with wrong w.
void reflected_normal(render::OffscreenRenderer& renderer, float node_scale = 1) {
    auto a = quad();
    add_texture(a, asset::TextureColorSpace::Linear, {128, 204, 230, 255});
    a.asset.materials[0].normal_texture = asset::TextureReference{};
    a.asset.nodes[0].scale = {node_scale, node_scale, node_scale};
    for (auto& v : a.asset.meshes[0].primitives[0].vertices) {
        for (auto& position : v.position) {
            position /= node_scale;
        }
    }
    good(renderer.upload_asset(a));
    auto s = make_scene(a.record.asset_id.to_string());
    for (auto e : s.entities()) {
        if (s.has<scene::AssetInstance>(e)) {
            s.transform(e).scale.x = -1;
        }
    }
    auto sun = take(s.create_entity("Analytical sun"));
    good(s.add_component<scene::DirectionalLight>(sun));
    s.component<scene::DirectionalLight>(sun).direction = {0, -1, -1};
    s.component<scene::DirectionalLight>(sun).intensity = 1;
    auto pixels = take(renderer.render(scene::SceneView(s), 1));
    const auto actual = pixels[((std::size_t{32} * 64U) + 32U) * 4U];
    near(actual, 137, "reflected analytical normal has wrong bitangent handedness");
    std::cout << "Analytical reflected normal: red=" << static_cast<int>(actual)
              << " expected=137\n";
    good(renderer.retire_asset(a.record.asset_id.to_string()));
    good(renderer.collect_assets());
}
int encoded(float linear) {
    const float mapped = linear / (1 + linear);
    const float value =
        mapped <= 0.0031308F ? mapped * 12.92F : (1.055F * std::pow(mapped, 1.0F / 2.4F)) - 0.055F;
    return static_cast<int>(std::lround(255 * value));
}
asset::LoadedAsset sampling_quad(bool vertical = false) {
    auto a = quad();
    add_texture(a, asset::TextureColorSpace::Linear, {0, 0, 0, 255});
    auto& material = a.asset.materials[0];
    material.base_color_factor = {0, 0, 0, 1};
    material.emissive_factor = {1, 1, 1};
    material.emissive_texture = asset::TextureReference{};
    auto& t = a.textures[0];
    t.width = 2;
    t.height = 2;
    t.rgba8.clear();
    for (std::size_t y = 0; y < 2; ++y) {
        for (std::size_t x = 0; x < 2; ++x) {
            const std::uint8_t v = (vertical ? y : x) == 0 ? 0 : 255;
            t.rgba8.insert(t.rgba8.end(), {v, v, v, 255});
        }
    }
    t.sampler.mag_filter = asset::Filter::Nearest;
    return a;
}
void sampler_probes(render::OffscreenRenderer& renderer) {
    constexpr std::array modes{
        asset::Wrap::Repeat,
        asset::Wrap::ClampToEdge,
        asset::Wrap::MirroredRepeat,
    };
    constexpr std::array<float, 4> coordinates{1.25F, -0.25F, 1.75F, -0.75F};
    // Rows are repeat/clamp/mirror. Each row differs over these four UVs.
    constexpr std::array<std::array<bool, 4>, 3> white{
        {
            {false, true, true, false},
            {true, false, true, false},
            {true, false, false, true},
        },
    };
    for (bool vertical : {false, true}) {
        for (std::size_t mode = 0; mode < modes.size(); ++mode) {
            for (std::size_t coordinate = 0; coordinate < coordinates.size(); ++coordinate) {
                const float uv = coordinates[coordinate];
                auto a = sampling_quad(vertical);
                a.textures[0].sampler.wrap_s = modes[mode];
                a.textures[0].sampler.wrap_t = modes[mode];
                for (auto& v : a.asset.meshes[0].primitives[0].vertices) {
                    v.uv0 =
                        vertical ? std::array<float, 2>{0.5F, uv} : std::array<float, 2>{uv, 0.5F};
                }
                near(red(renderer, a), white[mode][coordinate] ? 188 : 0,
                     "sampler wrap mode failed");
            }
        }
        // Both axes are out of range; distinct modes make an S/T swap observable.
        auto asymmetric = sampling_quad(vertical);
        asymmetric.textures[0].sampler.wrap_s = asset::Wrap::Repeat;
        asymmetric.textures[0].sampler.wrap_t = asset::Wrap::ClampToEdge;
        for (auto& v : asymmetric.asset.meshes[0].primitives[0].vertices) {
            v.uv0 = {1.25F, 1.25F};
        }
        near(red(renderer, asymmetric), vertical ? 188 : 0, "sampler wrap S/T modes swapped");
    }
    for (auto filter : {asset::Filter::Nearest, asset::Filter::Linear}) {
        auto a = sampling_quad();
        a.textures[0].sampler.mag_filter = filter;
        for (auto& v : a.asset.meshes[0].primitives[0].vertices) {
            v.uv0 = {0.3125F, 0.5F};
        }
        near(red(renderer, a), filter == asset::Filter::Nearest ? 0 : encoded(0.125F),
             "sampler magnification filter failed");
    }
    for (float lod : {0.25F, 0.75F}) {
        for (auto filter : {
                 asset::Filter::Nearest,
                 asset::Filter::Linear,
                 asset::Filter::NearestMipmapNearest,
                 asset::Filter::LinearMipmapNearest,
                 asset::Filter::NearestMipmapLinear,
                 asset::Filter::LinearMipmapLinear,
             }) {
            auto a = sampling_quad();
            a.textures[0].sampler.min_filter = filter;
            // At the center pixel u=0.3125 and du/dx=2^lod/width.
            // Its level-0 nearest/linear samples are 0 and 0.125; mip1=128/255.
            const float projection = 1 / std::tan(std::numbers::pi_v<float> / 6);
            for (auto& v : a.asset.meshes[0].primitives[0].vertices) {
                v.uv0 = {
                    0.3125F +
                        ((std::exp2(lod) / 2) * ((v.position[0] * projection * 32 / 3) - 0.5F)),
                    0.5F,
                };
            }
            const bool linear = filter == asset::Filter::Linear ||
                                filter == asset::Filter::LinearMipmapNearest ||
                                filter == asset::Filter::LinearMipmapLinear;
            float expected = linear ? 0.125F : 0;
            if (filter == asset::Filter::NearestMipmapLinear ||
                filter == asset::Filter::LinearMipmapLinear) {
                expected = (expected * (1 - lod)) + ((128.0F / 255) * lod);
            } else if (filter != asset::Filter::Nearest && filter != asset::Filter::Linear &&
                       lod > 0.5F) {
                expected = 128.0F / 255;
            }
            near(red(renderer, a), encoded(expected), "sampler minification/mip filter failed");
        }
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
    reflected_normal(renderer);
    // det(scale)=1e-42 underflows without shader normalization on FTZ hardware.
    reflected_normal(renderer, 1e-14F);
    sampler_probes(renderer);
    auto tiny = quad();
    tiny.asset.nodes[0].scale = {1e-6F, 1e-6F, 1e-6F};
    for (auto& v : tiny.asset.meshes[0].primitives[0].vertices) {
        for (auto& position : v.position) {
            position *= 1e6F;
        }
    }
    near(red(renderer, tiny), 48, "tiny uniform node scale rejected");
    auto collapsed = quad();
    auto node = collapsed.asset.nodes[0];
    node.scale = {0, 0, 0};
    collapsed.asset.nodes.push_back(node);
    collapsed.asset.scene_roots.push_back(1);
    near(red(renderer, collapsed), 48, "collapsed node prevented other geometry rendering");
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
    std::cout << "PBR color-space, alpha, sampler, reflected-normal, node-scale and full 4096-slot "
                 "probes passed\n";
}
} // namespace engine::test
