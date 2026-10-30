#include <engine/asset/tangents.hpp>

#include <mikktspace.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace engine::asset {
namespace {

struct TangentContext {
    // Non-owning views remain valid until genTangSpaceDefault returns. All
    // indices are checked before construction; callbacks never change buffers.
    // Collapsed-position welding invokes these getters millions of times, so
    // avoid repeated container access in unoptimized sanitizer/debug builds.
    const Vertex* vertices;
    const std::uint32_t* indices;
    std::size_t corner_count;
    std::vector<std::array<float, 4>> corners;
};

const TangentContext& context(const SMikkTSpaceContext* ctx) {
    return *static_cast<const TangentContext*>(ctx->m_pUserData);
}

const Vertex& vertex(const SMikkTSpaceContext* ctx, int face, int corner) {
    const auto& data = context(ctx);
    return data.vertices[data.indices[(static_cast<std::size_t>(face) * 3U) +
                                      static_cast<std::size_t>(corner)]];
}

bool needs_tangents(const ImportedAsset& asset, const Primitive& primitive) {
    return primitive.material.has_value() && *primitive.material < asset.materials.size() &&
           asset.materials[*primitive.material].normal_texture.has_value() &&
           !std::ranges::any_of(primitive.vertices, [](const Vertex& v) {
               return v.tangent[0] != 0.0F || v.tangent[1] != 0.0F || v.tangent[2] != 0.0F;
           });
}

core::Result<void> validate_primitive(const Primitive& primitive) {
    if (primitive.indices.empty() || primitive.indices.size() % 3U != 0U ||
        primitive.indices.size() > kMaxIndicesPerPrimitive ||
        primitive.vertices.size() > kMaxVerticesPerPrimitive ||
        std::ranges::any_of(
            primitive.indices,
            [&primitive](std::uint32_t index) { return index >= primitive.vertices.size(); }) ||
        std::ranges::any_of(primitive.vertices, [](const Vertex& v) {
            return !std::ranges::all_of(v.position, [](float x) { return std::isfinite(x); });
        })) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "invalid primitive for tangent generation"));
    }
    return {};
}

// Monotone Float32 codes, with signed zeros equal. Below 2^-103, coarsen
// spacing to the smallest normal float: FTZ/DAZ can collapse smaller spans
// during MikkTSpace's bbox/midpoint arithmetic. This also covers subnormals.
std::uint32_t position_code(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto magnitude = bits & 0x7FFFFFFFU;
    constexpr std::uint32_t transition = 0x0C000000U; // 2^-103
    constexpr std::uint32_t transition_code = 0x00800000U;
    const auto distance =
        magnitude >= transition
            ? magnitude - transition + transition_code
            : static_cast<std::uint32_t>(static_cast<double>(std::bit_cast<float>(magnitude)) /
                                         static_cast<double>(std::numeric_limits<float>::min()));
    return (bits & 0x80000000U) != 0U ? 0x80000000U - distance : 0x80000000U + distance;
}

std::uint64_t dense_pairs(std::uint32_t count) {
    // Counts are bounded by the validated 16M index limit; the product fits in uint64_t.
    static_assert(kMaxIndicesPerPrimitive <= std::numeric_limits<std::uint32_t>::max());
    return count > 64U ? (static_cast<std::uint64_t>(count) * (count - 1U)) / 2U : 0U;
}

core::Result<std::uint64_t> weld_work(const Primitive& primitive, std::uint64_t consumed,
                                      std::size_t mesh, std::size_t primitive_index) {
    std::uint64_t maximum = 0;
    for (std::uint32_t shift = 0; shift < 8U; ++shift) {
        // One balanced tree at a time bounds memory and avoids attacker-chosen
        // hash collisions. The eight offsets cover neighboring cell boundaries.
        std::map<std::array<std::uint32_t, 3>, std::uint32_t> counts;
        std::uint64_t work = 0;
        for (const auto index : primitive.indices) {
            std::array<std::uint32_t, 3> key{};
            const auto& position = primitive.vertices[index].position;
            for (std::size_t axis = 0; axis < key.size(); ++axis) {
                key[axis] = (position_code(position[axis]) + ((shift >> axis) & 1U)) / 2U;
            }
            auto& count = counts[key];
            const auto before = dense_pairs(count);
            ++count;
            work += dense_pairs(count) - before;
            if (work > kMaxTangentWeldPairs - consumed) {
                const auto pointer = "/meshes/" + std::to_string(mesh) + "/primitives/" +
                                     std::to_string(primitive_index);
                return core::make_failure<std::uint64_t>(core::make_error(
                    core::ErrorCode::Validation, "tangent generation exceeds the weld-work budget",
                    "Export tangents in Blender or simplify dense geometry.",
                    {
                        {"estimatedWork", consumed + work},
                        {"limit", kMaxTangentWeldPairs},
                        {"meshIndex", mesh},
                        {"primitiveIndex", primitive_index},
                        {"jsonPointer", pointer},
                    }));
            }
        }
        maximum = std::max(maximum, work);
    }
    return maximum;
}

core::Result<void> generate(Primitive& primitive) {
    // Whole-asset validation and budget admission finish before any generation.
    TangentContext data{
        primitive.vertices.data(),
        primitive.indices.data(),
        primitive.indices.size(),
        std::vector<std::array<float, 4>>(primitive.indices.size()),
    };
    SMikkTSpaceInterface callbacks{};
    callbacks.m_getNumFaces = [](const SMikkTSpaceContext* ctx) {
        return static_cast<int>(context(ctx).corner_count / 3U);
    };
    callbacks.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
    callbacks.m_getPosition = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        const auto* value = vertex(ctx, face, corner).position.data();
        out[0] = value[0];
        out[1] = value[1];
        out[2] = value[2];
    };
    callbacks.m_getNormal = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        const auto* value = vertex(ctx, face, corner).normal.data();
        out[0] = value[0];
        out[1] = value[1];
        out[2] = value[2];
    };
    callbacks.m_getTexCoord = [](const SMikkTSpaceContext* ctx, float out[], int face, int corner) {
        const auto* value = vertex(ctx, face, corner).uv0.data();
        out[0] = value[0];
        out[1] = value[1];
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
    std::uint64_t consumed = 0;
    for (std::size_t mesh = 0; mesh < asset.meshes.size(); ++mesh) {
        const auto& primitives = asset.meshes[mesh].primitives;
        for (std::size_t primitive = 0; primitive < primitives.size(); ++primitive) {
            const auto& item = primitives[primitive];
            if (!needs_tangents(asset, item)) {
                continue;
            }
            if (auto valid = validate_primitive(item); !valid) {
                return valid;
            }
            auto work = weld_work(item, consumed, mesh, primitive);
            if (!work) {
                return core::make_failure<void>(work.error());
            }
            consumed += *work;
        }
    }
    for (auto& mesh : asset.meshes) {
        for (auto& primitive : mesh.primitives) {
            if (!needs_tangents(asset, primitive)) {
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
