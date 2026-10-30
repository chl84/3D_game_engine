#include <engine/asset/tangents.hpp>

#include <mikktspace.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace engine::asset {
namespace {

struct TangentContext {
    const Primitive& primitive;
    std::vector<std::array<float, 4>> corners;
};

const TangentContext& context(const SMikkTSpaceContext* ctx) {
    return *static_cast<const TangentContext*>(ctx->m_pUserData);
}

const Vertex& vertex(const SMikkTSpaceContext* ctx, int face, int corner) {
    const auto& primitive = context(ctx).primitive;
    return primitive.vertices[primitive.indices[(static_cast<std::size_t>(face) * 3U) +
                                                static_cast<std::size_t>(corner)]];
}

core::Result<void> generate(Primitive& primitive) {
    if (primitive.indices.empty() || primitive.indices.size() % 3U != 0U ||
        primitive.indices.size() > kMaxIndicesPerPrimitive ||
        primitive.vertices.size() > kMaxVerticesPerPrimitive ||
        std::ranges::any_of(primitive.indices, [&primitive](std::uint32_t index) {
            return index >= primitive.vertices.size();
        })) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "invalid primitive for tangent generation"));
    }
    TangentContext data{primitive, std::vector<std::array<float, 4>>(primitive.indices.size())};
    SMikkTSpaceInterface callbacks{};
    callbacks.m_getNumFaces = [](const SMikkTSpaceContext* ctx) {
        return static_cast<int>(context(ctx).primitive.indices.size() / 3U);
    };
    callbacks.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
    callbacks.m_getPosition = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        std::ranges::copy(vertex(ctx, face, corner).position, out);
    };
    callbacks.m_getNormal = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        std::ranges::copy(vertex(ctx, face, corner).normal, out);
    };
    callbacks.m_getTexCoord = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        std::ranges::copy(vertex(ctx, face, corner).uv0, out);
    };
    callbacks.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx, const float tangent[],
                                    float sign, int face, int corner) {
        auto& output = *static_cast<TangentContext*>(ctx->m_pUserData);
        output.corners[(static_cast<std::size_t>(face) * 3U) + static_cast<std::size_t>(corner)] =
            // glTF uses a top-left texture origin; its stored tangent.w is
            // the opposite of the upstream MikkTSpace sign for these UVs.
            {tangent[0], tangent[1], tangent[2], -sign};
    };
    const SMikkTSpaceContext ctx{&callbacks, &data};
    if (genTangSpaceDefault(&ctx) == 0) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Validation,
                                                         "MikkTSpace could not generate tangents"));
    }

    // Upstream returns per-corner results. Never average them back through the
    // old index list: mirrored UVs can require different signs at one vertex.
    using Key = std::pair<std::uint32_t, std::array<float, 4>>;
    std::map<Key, std::uint32_t> remap;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    indices.reserve(primitive.indices.size());
    for (std::size_t corner = 0; corner < data.corners.size(); ++corner) {
        const auto& tangent = data.corners[corner];
        if (!std::ranges::all_of(tangent, [](float value) { return std::isfinite(value); })) {
            return core::make_failure<void>(core::make_error(
                core::ErrorCode::Validation, "MikkTSpace produced a non-finite tangent"));
        }
        const Key key{primitive.indices[corner], tangent};
        auto [it, inserted] = remap.try_emplace(key, static_cast<std::uint32_t>(vertices.size()));
        if (inserted) {
            if (vertices.size() >= kMaxVerticesPerPrimitive) {
                return core::make_failure<void>(core::make_error(
                    core::ErrorCode::Validation, "tangent splitting exceeds the vertex limit"));
            }
            auto output = primitive.vertices[key.first];
            output.tangent = tangent;
            vertices.push_back(output);
        }
        indices.push_back(it->second);
    }
    primitive.vertices = std::move(vertices);
    primitive.indices = std::move(indices);
    return {};
}

} // namespace

core::Result<void> generate_missing_tangents(ImportedAsset& asset) {
    for (auto& mesh : asset.meshes) {
        for (auto& primitive : mesh.primitives) {
            if (!primitive.material.has_value() || *primitive.material >= asset.materials.size() ||
                !asset.materials[*primitive.material].normal_texture.has_value()) {
                continue;
            }
            if (std::ranges::any_of(primitive.vertices, [](const Vertex& v) {
                    return v.tangent[0] != 0.0F || v.tangent[1] != 0.0F || v.tangent[2] != 0.0F;
                })) {
                continue;
            }
            if (auto result = generate(primitive); !result) {
                return result;
            }
        }
    }
    return {};
}

} // namespace engine::asset
