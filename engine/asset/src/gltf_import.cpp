#include <engine/asset/gltf_import.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace engine::asset {
namespace {

using core::Error;
using core::ErrorCode;
using core::Result;

[[nodiscard]] Error make_import_error(ErrorCode code, std::string message, std::string pointer,
                                      nlohmann::json details = nlohmann::json::object()) {
    details["pointer"] = std::move(pointer);
    return core::make_error(code, std::move(message), {}, std::move(details));
}

[[nodiscard]] Error from_fastgltf(fastgltf::Error error, std::string_view source) {
    using fastgltf::Error;
    ErrorCode code = ErrorCode::Validation;
    switch (error) {
    case Error::MissingExtensions:
    case Error::UnknownRequiredExtension:
    case Error::UnsupportedVersion:
        code = ErrorCode::Unsupported;
        break;
    case Error::InvalidPath:
    case Error::MissingExternalBuffer:
        code = ErrorCode::Io;
        break;
    default:
        break;
    }
    std::string pointer;
    if (error == Error::MissingExtensions || error == Error::UnknownRequiredExtension) {
        pointer = "/extensionsRequired";
    } else if (error == Error::UnsupportedVersion) {
        pointer = "/asset/version";
    }
    return core::make_error(
        code,
        "cannot parse GLB '" + std::string(source) +
            "': " + std::string(fastgltf::getErrorMessage(error)),
        "Export the asset as a self-contained binary glTF (.glb) from the source tool.",
        {
            {"importerError", std::string(fastgltf::getErrorName(error))},
            {"pointer", std::move(pointer)},
        });
}

[[nodiscard]] Result<std::span<const std::byte>> buffer_bytes(const fastgltf::Asset& gltf,
                                                              std::size_t index) {
    if (index >= gltf.buffers.size()) {
        return core::make_failure<std::span<const std::byte>>(
            make_import_error(ErrorCode::Validation, "buffer index is out of range", "/buffers",
                              {{"buffer", index}, {"count", gltf.buffers.size()}}));
    }
    return std::visit(
        [index](const auto& data) -> Result<std::span<const std::byte>> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, fastgltf::sources::Array>) {
                return std::span<const std::byte>(data.bytes.data(), data.bytes.size());
            } else if constexpr (std::is_same_v<T, fastgltf::sources::ByteView>) {
                return data.bytes;
            } else if constexpr (std::is_same_v<T, fastgltf::sources::Vector>) {
                return std::span<const std::byte>(data.bytes.data(), data.bytes.size());
            } else {
                return core::make_failure<std::span<const std::byte>>(make_import_error(
                    ErrorCode::Unsupported,
                    "external or custom buffers are not supported by the M3 importer",
                    "/buffers/" + std::to_string(index), {{"buffer", index}}));
            }
        },
        gltf.buffers[index].data);
}

[[nodiscard]] std::optional<ImageMimeType> image_mime_type(fastgltf::MimeType mime) {
    switch (mime) {
    case fastgltf::MimeType::PNG:
        return ImageMimeType::Png;
    case fastgltf::MimeType::JPEG:
        return ImageMimeType::Jpeg;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] Result<std::vector<std::byte>> image_bytes(const fastgltf::Asset& gltf,
                                                         std::size_t index) {
    if (index >= gltf.images.size()) {
        return core::make_failure<std::vector<std::byte>>(
            make_import_error(ErrorCode::Validation, "image index is out of range", "/images",
                              {{"image", index}, {"count", gltf.images.size()}}));
    }
    const auto& image = gltf.images[index];
    return std::visit(
        [&](const auto& data) -> Result<std::vector<std::byte>> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>) {
                const auto view_index = data.bufferViewIndex;
                if (view_index >= gltf.bufferViews.size()) {
                    return core::make_failure<std::vector<std::byte>>(make_import_error(
                        ErrorCode::Validation, "image buffer view is out of range",
                        "/images/" + std::to_string(index),
                        {{"image", index}, {"bufferView", view_index}}));
                }
                const auto& view = gltf.bufferViews[view_index];
                auto bytes = buffer_bytes(gltf, view.bufferIndex);
                if (!bytes) {
                    return core::make_failure<std::vector<std::byte>>(bytes.error());
                }
                if (view.byteOffset > bytes->size() ||
                    view.byteLength > bytes->size() - view.byteOffset) {
                    return core::make_failure<std::vector<std::byte>>(make_import_error(
                        ErrorCode::Validation, "image buffer view exceeds the buffer bounds",
                        "/images", {{"image", index}, {"bufferView", view_index}}));
                }
                const auto slice = bytes->subspan(view.byteOffset, view.byteLength);
                return std::vector<std::byte>(slice.begin(), slice.end());
            } else if constexpr (std::is_same_v<T, fastgltf::sources::Array>) {
                return std::vector<std::byte>(data.bytes.begin(), data.bytes.end());
            } else if constexpr (std::is_same_v<T, fastgltf::sources::ByteView>) {
                return std::vector<std::byte>(data.bytes.begin(), data.bytes.end());
            } else if constexpr (std::is_same_v<T, fastgltf::sources::Vector>) {
                return data.bytes;
            } else {
                return core::make_failure<std::vector<std::byte>>(make_import_error(
                    ErrorCode::Unsupported, "external images are not supported by the M3 importer",
                    "/images/" + std::to_string(index), {{"image", index}}));
            }
        },
        image.data);
}

[[nodiscard]] Result<std::optional<TextureReference>>
texture_reference(const fastgltf::TextureInfo* info, const fastgltf::Asset& gltf,
                  std::string pointer) {
    if (info == nullptr) {
        return std::optional<TextureReference>{};
    }
    if (info->textureIndex >= gltf.textures.size()) {
        return core::make_failure<std::optional<TextureReference>>(make_import_error(
            ErrorCode::Validation, "material texture index is out of range", std::move(pointer),
            {{"texture", info->textureIndex}, {"count", gltf.textures.size()}}));
    }
    if (info->texCoordIndex != 0) {
        return core::make_failure<std::optional<TextureReference>>(make_import_error(
            ErrorCode::Unsupported, "only TEXCOORD_0 texture coordinates are supported",
            std::move(pointer), {{"texCoord", info->texCoordIndex}}));
    }
    if (info->transform != nullptr) {
        return core::make_failure<std::optional<TextureReference>>(make_import_error(
            ErrorCode::Unsupported, "KHR_texture_transform is not supported", std::move(pointer)));
    }
    return std::optional<TextureReference>{
        TextureReference{info->textureIndex, static_cast<std::uint32_t>(info->texCoordIndex)}};
}

template <typename T>
[[nodiscard]] Result<std::optional<TextureReference>>
texture_reference(const std::optional<T>& info, const fastgltf::Asset& gltf, std::string pointer) {
    if (!info.has_value()) {
        return std::optional<TextureReference>{};
    }
    return texture_reference(&*info, gltf, std::move(pointer));
}

// fastgltf's optional validate() walks animation and accessor indices without
// bounds checks in 0.9.1. The importer validates every index it uses itself and
// deliberately does not call validate(); animation data is not read at all, but
// its references are still checked so a malformed file fails cleanly.
[[nodiscard]] Result<void> validate_animation_references(const fastgltf::Asset& gltf) {
    for (std::size_t animation_index = 0; animation_index < gltf.animations.size();
         ++animation_index) {
        const auto& animation = gltf.animations[animation_index];
        for (std::size_t channel_index = 0; channel_index < animation.channels.size();
             ++channel_index) {
            const auto& channel = animation.channels[channel_index];
            if (channel.samplerIndex >= animation.samplers.size()) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "animation channel sampler is out of range",
                    "/animations/" + std::to_string(animation_index) + "/channels/" +
                        std::to_string(channel_index) + "/sampler",
                    {
                        {"animation", animation_index},
                        {"channel", channel_index},
                        {"sampler", channel.samplerIndex},
                    }));
            }
            if (channel.nodeIndex.has_value() && *channel.nodeIndex >= gltf.nodes.size()) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "animation channel node is out of range",
                    "/animations/" + std::to_string(animation_index) + "/channels/" +
                        std::to_string(channel_index) + "/target/node",
                    {
                        {"animation", animation_index},
                        {"channel", channel_index},
                        {"node", *channel.nodeIndex},
                    }));
            }
        }
        for (std::size_t sampler_index = 0; sampler_index < animation.samplers.size();
             ++sampler_index) {
            const auto& sampler = animation.samplers[sampler_index];
            for (const auto& [accessor, name] : {
                     std::pair{sampler.inputAccessor, "input"},
                     std::pair{sampler.outputAccessor, "output"},
                 }) {
                if (accessor >= gltf.accessors.size()) {
                    return core::make_failure<void>(make_import_error(
                        ErrorCode::Validation, "animation sampler accessor is out of range",
                        "/animations/" + std::to_string(animation_index) + "/samplers/" +
                            std::to_string(sampler_index) + "/" + name,
                        {
                            {"animation", animation_index},
                            {"sampler", sampler_index},
                            {"accessor", accessor},
                        }));
                }
                const auto& animation_accessor = gltf.accessors[accessor];
                if (animation_accessor.bufferViewIndex.has_value() &&
                    *animation_accessor.bufferViewIndex >= gltf.bufferViews.size()) {
                    return core::make_failure<void>(make_import_error(
                        ErrorCode::Validation, "animation accessor buffer view is out of range",
                        "/accessors/" + std::to_string(accessor) + "/bufferView",
                        {
                            {"accessor", accessor},
                            {"bufferView", *animation_accessor.bufferViewIndex},
                        }));
                }
            }
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_buffer_layout(const fastgltf::Asset& gltf) {
    std::vector<std::span<const std::byte>> buffers;
    buffers.reserve(gltf.buffers.size());
    for (std::size_t index = 0; index < gltf.buffers.size(); ++index) {
        auto bytes = buffer_bytes(gltf, index);
        if (!bytes) {
            return core::make_failure<void>(bytes.error());
        }
        const auto& buffer = gltf.buffers[index];
        if (bytes->size() < buffer.byteLength) {
            return core::make_failure<void>(make_import_error(
                ErrorCode::Validation, "buffer is shorter than its declared byte length",
                "/buffers/" + std::to_string(index),
                {
                    {"buffer", index},
                    {"declared", buffer.byteLength},
                    {"available", bytes->size()},
                }));
        }
        buffers.push_back(*bytes);
    }

    for (std::size_t index = 0; index < gltf.bufferViews.size(); ++index) {
        const auto& view = gltf.bufferViews[index];
        if (view.bufferIndex >= buffers.size()) {
            return core::make_failure<void>(
                make_import_error(ErrorCode::Validation, "buffer view references a missing buffer",
                                  "/bufferViews/" + std::to_string(index),
                                  {{"bufferView", index}, {"buffer", view.bufferIndex}}));
        }
        const auto available = buffers[view.bufferIndex];
        const auto& buffer = gltf.buffers[view.bufferIndex];
        if (view.byteOffset > available.size() ||
            view.byteLength > available.size() - view.byteOffset ||
            view.byteOffset > buffer.byteLength ||
            view.byteLength > buffer.byteLength - view.byteOffset) {
            return core::make_failure<void>(make_import_error(
                ErrorCode::Validation, "buffer view exceeds the actual buffer data",
                "/bufferViews/" + std::to_string(index),
                {
                    {"bufferView", index},
                    {"buffer", view.bufferIndex},
                    {"offset", view.byteOffset},
                    {"length", view.byteLength},
                    {"declared", buffer.byteLength},
                    {"available", available.size()},
                }));
        }
    }

    for (std::size_t index = 0; index < gltf.accessors.size(); ++index) {
        const auto& accessor = gltf.accessors[index];
        const std::size_t element_size =
            fastgltf::getElementByteSize(accessor.type, accessor.componentType);
        if (element_size == 0U) {
            return core::make_failure<void>(
                make_import_error(ErrorCode::Validation, "accessor has an invalid element type",
                                  "/accessors/" + std::to_string(index), {{"accessor", index}}));
        }
        if (accessor.bufferViewIndex.has_value()) {
            if (*accessor.bufferViewIndex >= gltf.bufferViews.size()) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "accessor references a missing buffer view",
                    "/accessors/" + std::to_string(index),
                    {{"accessor", index}, {"bufferView", *accessor.bufferViewIndex}}));
            }
            const auto& view = gltf.bufferViews[*accessor.bufferViewIndex];
            const std::size_t stride = view.byteStride.value_or(element_size);
            if (stride < element_size) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "buffer view stride is smaller than the element size",
                    "/bufferViews/" + std::to_string(*accessor.bufferViewIndex),
                    {
                        {"bufferView", *accessor.bufferViewIndex},
                        {"stride", stride},
                        {"elementSize", element_size},
                    }));
            }
            std::size_t required = 0;
            if (accessor.count > 0U) {
                if (accessor.count - 1U >
                    (std::numeric_limits<std::size_t>::max() - element_size) / stride) {
                    return core::make_failure<void>(
                        make_import_error(ErrorCode::Validation, "accessor byte range overflows",
                                          "/accessors", {{"accessor", index}}));
                }
                required = (stride * (accessor.count - 1U)) + element_size;
            }
            if (accessor.byteOffset > view.byteLength ||
                required > view.byteLength - accessor.byteOffset) {
                return core::make_failure<void>(
                    make_import_error(ErrorCode::Validation, "accessor exceeds its buffer view",
                                      "/accessors/" + std::to_string(index),
                                      {
                                          {"accessor", index},
                                          {"bufferView", *accessor.bufferViewIndex},
                                          {"offset", accessor.byteOffset},
                                          {"required", required},
                                          {"available", view.byteLength},
                                      }));
            }
        }
        if (accessor.sparse.has_value()) {
            const auto& sparse = *accessor.sparse;
            if (sparse.indicesBufferView >= gltf.bufferViews.size() ||
                sparse.valuesBufferView >= gltf.bufferViews.size()) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "sparse accessor references a missing buffer view",
                    "/accessors/" + std::to_string(index), {{"accessor", index}}));
            }
            const std::size_t index_size = fastgltf::getElementByteSize(
                fastgltf::AccessorType::Scalar, sparse.indexComponentType);
            const std::size_t index_stride =
                gltf.bufferViews[sparse.indicesBufferView].byteStride.value_or(index_size);
            const std::size_t value_stride =
                gltf.bufferViews[sparse.valuesBufferView].byteStride.value_or(element_size);
            const auto fits = [](std::size_t count, std::size_t stride, std::size_t size,
                                 std::size_t offset, std::size_t length) {
                if (count == 0U) {
                    return offset <= length;
                }
                if (stride == 0U ||
                    count - 1U > (std::numeric_limits<std::size_t>::max() - size) / stride) {
                    return false;
                }
                const std::size_t required = (stride * (count - 1U)) + size;
                return offset <= length && required <= length - offset;
            };
            if (!fits(sparse.count, index_stride, index_size, sparse.indicesByteOffset,
                      gltf.bufferViews[sparse.indicesBufferView].byteLength) ||
                !fits(sparse.count, value_stride, element_size, sparse.valuesByteOffset,
                      gltf.bufferViews[sparse.valuesBufferView].byteLength)) {
                return core::make_failure<void>(make_import_error(
                    ErrorCode::Validation, "sparse accessor exceeds its buffer views",
                    "/accessors/" + std::to_string(index), {{"accessor", index}}));
            }
        }
    }
    return {};
}

[[nodiscard]] Result<std::vector<Vertex>> read_vertices(const fastgltf::Asset& gltf,
                                                        const fastgltf::Primitive& primitive,
                                                        std::size_t mesh_index,
                                                        std::size_t primitive_index) {
    const std::string pointer =
        "/meshes/" + std::to_string(mesh_index) + "/primitives/" + std::to_string(primitive_index);
    const fastgltf::Attribute* position = primitive.findAttribute("POSITION");
    if (position == primitive.attributes.end()) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "primitive is missing the POSITION attribute",
                              pointer, {{"primitive", primitive_index}}));
    }
    const fastgltf::Attribute* normal = primitive.findAttribute("NORMAL");
    if (normal == primitive.attributes.end()) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "primitive is missing the NORMAL attribute",
                              pointer, {{"primitive", primitive_index}}));
    }
    const fastgltf::Attribute* tangent = primitive.findAttribute("TANGENT");
    const fastgltf::Attribute* texcoord = primitive.findAttribute("TEXCOORD_0");
    for (const auto& attribute : primitive.attributes) {
        const bool supported = attribute.name == "POSITION" || attribute.name == "NORMAL" ||
                               attribute.name == "TANGENT" || attribute.name == "TEXCOORD_0";
        if (!supported) {
            const std::string attribute_name(attribute.name);
            return core::make_failure<std::vector<Vertex>>(make_import_error(
                ErrorCode::Unsupported, "unsupported vertex attribute '" + attribute_name + "'",
                pointer, {{"primitive", primitive_index}, {"attribute", attribute_name}}));
        }
    }

    for (const auto* attribute : {&*position, &*normal}) {
        if (attribute->accessorIndex >= gltf.accessors.size()) {
            return core::make_failure<std::vector<Vertex>>(make_import_error(
                ErrorCode::Validation, "vertex attribute accessor is out of range", pointer,
                {{"primitive", primitive_index}, {"accessor", attribute->accessorIndex}}));
        }
    }
    if (tangent != primitive.attributes.end() && tangent->accessorIndex >= gltf.accessors.size()) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "TANGENT accessor is out of range", pointer,
                              {{"primitive", primitive_index}}));
    }
    if (texcoord != primitive.attributes.end() &&
        texcoord->accessorIndex >= gltf.accessors.size()) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "TEXCOORD_0 accessor is out of range", pointer,
                              {{"primitive", primitive_index}}));
    }
    const auto& position_accessor = gltf.accessors[position->accessorIndex];
    if (position_accessor.type != fastgltf::AccessorType::Vec3 ||
        normal->accessorIndex >= gltf.accessors.size() ||
        gltf.accessors[normal->accessorIndex].type != fastgltf::AccessorType::Vec3) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "POSITION and NORMAL must be VEC3 accessors",
                              pointer, {{"primitive", primitive_index}}));
    }
    if (tangent != primitive.attributes.end() &&
        gltf.accessors[tangent->accessorIndex].type != fastgltf::AccessorType::Vec4) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "TANGENT must be a VEC4 accessor", pointer,
                              {{"primitive", primitive_index}}));
    }
    if (texcoord != primitive.attributes.end() &&
        gltf.accessors[texcoord->accessorIndex].type != fastgltf::AccessorType::Vec2) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "TEXCOORD_0 must be a VEC2 accessor", pointer,
                              {{"primitive", primitive_index}}));
    }
    if (position_accessor.count == 0) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "primitive has no vertices", pointer,
                              {{"primitive", primitive_index}}));
    }
    if (position_accessor.count > kMaxVerticesPerPrimitive) {
        return core::make_failure<std::vector<Vertex>>(
            make_import_error(ErrorCode::Validation, "primitive exceeds the vertex limit", pointer,
                              {
                                  {"primitive", primitive_index},
                                  {"vertices", position_accessor.count},
                                  {"limit", kMaxVerticesPerPrimitive},
                              }));
    }
    const auto require_float = [&](const fastgltf::Attribute& attribute) {
        return gltf.accessors[attribute.accessorIndex].componentType ==
                   fastgltf::ComponentType::Float &&
               gltf.accessors[attribute.accessorIndex].count == position_accessor.count;
    };
    if (!require_float(*position)) {
        return core::make_failure<std::vector<Vertex>>(make_import_error(
            ErrorCode::Unsupported, "POSITION must be a non-quantized float accessor", pointer,
            {{"primitive", primitive_index}}));
    }
    if (!require_float(*normal)) {
        return core::make_failure<std::vector<Vertex>>(make_import_error(
            ErrorCode::Unsupported, "NORMAL must be a non-quantized float accessor", pointer,
            {{"primitive", primitive_index}}));
    }
    if (tangent != primitive.attributes.end() && !require_float(*tangent)) {
        return core::make_failure<std::vector<Vertex>>(make_import_error(
            ErrorCode::Unsupported, "TANGENT must be a non-quantized float accessor", pointer,
            {{"primitive", primitive_index}}));
    }
    if (texcoord != primitive.attributes.end() && !require_float(*texcoord)) {
        return core::make_failure<std::vector<Vertex>>(make_import_error(
            ErrorCode::Unsupported, "TEXCOORD_0 must be a non-quantized float accessor", pointer,
            {{"primitive", primitive_index}}));
    }

    std::vector<Vertex> vertices(position_accessor.count);
    std::size_t vertex_index = 0;
    fastgltf::iterateAccessor<fastgltf::math::fvec3>(
        gltf, position_accessor, [&](const fastgltf::math::fvec3& value) {
            vertices[vertex_index].position = {value[0], value[1], value[2]};
            ++vertex_index;
        });
    vertex_index = 0;
    fastgltf::iterateAccessor<fastgltf::math::fvec3>(
        gltf, gltf.accessors[normal->accessorIndex], [&](const fastgltf::math::fvec3& value) {
            vertices[vertex_index].normal = {value[0], value[1], value[2]};
            ++vertex_index;
        });
    if (tangent != primitive.attributes.end()) {
        vertex_index = 0;
        fastgltf::iterateAccessor<fastgltf::math::fvec4>(
            gltf, gltf.accessors[tangent->accessorIndex], [&](const fastgltf::math::fvec4& value) {
                vertices[vertex_index].tangent = {value[0], value[1], value[2], value[3]};
                ++vertex_index;
            });
    }
    if (texcoord != primitive.attributes.end()) {
        vertex_index = 0;
        fastgltf::iterateAccessor<fastgltf::math::fvec2>(
            gltf, gltf.accessors[texcoord->accessorIndex], [&](const fastgltf::math::fvec2& value) {
                vertices[vertex_index].uv0 = {value[0], value[1]};
                ++vertex_index;
            });
    }

    const auto finite = [](float value) { return std::isfinite(value); };
    for (const auto& vertex : vertices) {
        const bool valid = std::ranges::all_of(vertex.position, finite) &&
                           std::ranges::all_of(vertex.normal, finite) &&
                           std::ranges::all_of(vertex.tangent, finite) &&
                           std::ranges::all_of(vertex.uv0, finite);
        if (!valid) {
            return core::make_failure<std::vector<Vertex>>(make_import_error(
                ErrorCode::Validation, "primitive contains non-finite vertex data", pointer,
                {{"primitive", primitive_index}}));
        }
    }
    return vertices;
}

[[nodiscard]] Result<std::vector<std::uint32_t>>
read_indices(const fastgltf::Asset& gltf, const fastgltf::Primitive& primitive,
             std::size_t mesh_index, std::size_t primitive_index, std::size_t vertex_count) {
    const std::string pointer =
        "/meshes/" + std::to_string(mesh_index) + "/primitives/" + std::to_string(primitive_index);
    if (!primitive.indicesAccessor.has_value()) {
        return core::make_failure<std::vector<std::uint32_t>>(
            make_import_error(ErrorCode::Validation, "primitive has no index accessor", pointer,
                              {{"primitive", primitive_index}}));
    }
    if (*primitive.indicesAccessor >= gltf.accessors.size()) {
        return core::make_failure<std::vector<std::uint32_t>>(make_import_error(
            ErrorCode::Validation, "index accessor is out of range", pointer,
            {{"primitive", primitive_index}, {"accessor", *primitive.indicesAccessor}}));
    }
    const auto& accessor = gltf.accessors[*primitive.indicesAccessor];
    if (accessor.type != fastgltf::AccessorType::Scalar ||
        (accessor.componentType != fastgltf::ComponentType::UnsignedByte &&
         accessor.componentType != fastgltf::ComponentType::UnsignedShort &&
         accessor.componentType != fastgltf::ComponentType::UnsignedInt)) {
        return core::make_failure<std::vector<std::uint32_t>>(make_import_error(
            ErrorCode::Unsupported, "indices must be unsigned 8/16/32-bit scalars", pointer,
            {{"primitive", primitive_index}}));
    }
    if (accessor.count > kMaxIndicesPerPrimitive) {
        return core::make_failure<std::vector<std::uint32_t>>(
            make_import_error(ErrorCode::Validation, "primitive exceeds the index limit", pointer,
                              {
                                  {"primitive", primitive_index},
                                  {"indices", accessor.count},
                                  {"limit", kMaxIndicesPerPrimitive},
                              }));
    }
    if ((accessor.count % 3U) != 0U) {
        return core::make_failure<std::vector<std::uint32_t>>(make_import_error(
            ErrorCode::Validation, "triangle primitive index count is not a multiple of three",
            pointer, {{"primitive", primitive_index}, {"indices", accessor.count}}));
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(accessor.count);
    fastgltf::iterateAccessor<std::uint32_t>(
        gltf, accessor, [&](std::uint32_t value) { indices.push_back(value); });
    for (const std::uint32_t index : indices) {
        if (static_cast<std::size_t>(index) >= vertex_count) {
            return core::make_failure<std::vector<std::uint32_t>>(make_import_error(
                ErrorCode::Validation, "primitive index is out of range", pointer,
                {{"primitive", primitive_index}, {"index", index}, {"vertices", vertex_count}}));
        }
    }
    return indices;
}

[[nodiscard]] Bounds compute_bounds(const std::vector<Vertex>& vertices) {
    Bounds bounds{
        {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        },
        {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        },
    };
    for (const auto& vertex : vertices) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            bounds.min[axis] = std::min(bounds.min[axis], vertex.position[axis]);
            bounds.max[axis] = std::max(bounds.max[axis], vertex.position[axis]);
        }
    }
    return bounds;
}

[[nodiscard]] Result<Material> convert_material(const fastgltf::Material& source,
                                                const fastgltf::Asset& gltf,
                                                std::size_t material_index) {
    if (source.alphaMode == fastgltf::AlphaMode::Blend) {
        return core::make_failure<Material>(make_import_error(
            ErrorCode::Unsupported, "blended transparency is not supported until M5",
            "/materials/" + std::to_string(material_index), {{"material", material_index}}));
    }
    Material material;
    material.name = std::string(source.name);
    material.base_color_factor = {
        source.pbrData.baseColorFactor[0],
        source.pbrData.baseColorFactor[1],
        source.pbrData.baseColorFactor[2],
        source.pbrData.baseColorFactor[3],
    };
    material.metallic_factor = source.pbrData.metallicFactor;
    material.roughness_factor = source.pbrData.roughnessFactor;
    material.emissive_factor = {
        source.emissiveFactor[0],
        source.emissiveFactor[1],
        source.emissiveFactor[2],
    };
    material.alpha_mode =
        source.alphaMode == fastgltf::AlphaMode::Mask ? AlphaMode::Mask : AlphaMode::Opaque;
    material.alpha_cutoff = source.alphaCutoff;
    material.double_sided = source.doubleSided;
    const auto in_unit = [](float value) { return value >= 0.0F && value <= 1.0F; };
    const bool factors_valid =
        std::ranges::all_of(material.base_color_factor, in_unit) &&
        in_unit(material.metallic_factor) && in_unit(material.roughness_factor) &&
        std::ranges::all_of(material.emissive_factor, in_unit) && in_unit(material.alpha_cutoff);
    if (!factors_valid) {
        return core::make_failure<Material>(make_import_error(
            ErrorCode::Validation, "material factors are outside the glTF 0..1 range",
            "/materials/" + std::to_string(material_index), {{"material", material_index}}));
    }
    material.normal_scale = source.normalTexture.has_value() ? source.normalTexture->scale : 1.0F;
    material.occlusion_strength =
        source.occlusionTexture.has_value() ? source.occlusionTexture->strength : 1.0F;

    const std::string base = "/materials/" + std::to_string(material_index);
    auto base_color =
        texture_reference(source.pbrData.baseColorTexture, gltf, base + "/baseColorTexture");
    if (!base_color) {
        return core::make_failure<Material>(base_color.error());
    }
    material.base_color_texture = *base_color;
    auto metallic_roughness = texture_reference(source.pbrData.metallicRoughnessTexture, gltf,
                                                base + "/metallicRoughnessTexture");
    if (!metallic_roughness) {
        return core::make_failure<Material>(metallic_roughness.error());
    }
    material.metallic_roughness_texture = *metallic_roughness;
    auto normal = texture_reference(source.normalTexture, gltf, base + "/normalTexture");
    if (!normal) {
        return core::make_failure<Material>(normal.error());
    }
    material.normal_texture = *normal;
    auto occlusion = texture_reference(source.occlusionTexture, gltf, base + "/occlusionTexture");
    if (!occlusion) {
        return core::make_failure<Material>(occlusion.error());
    }
    material.occlusion_texture = *occlusion;
    auto emissive = texture_reference(source.emissiveTexture, gltf, base + "/emissiveTexture");
    if (!emissive) {
        return core::make_failure<Material>(emissive.error());
    }
    material.emissive_texture = *emissive;
    return material;
}

} // namespace

std::vector<WorldTransform> world_transforms(const ImportedAsset& asset) {
    struct Quatd {
        double x{0.0};
        double y{0.0};
        double z{0.0};
        double w{1.0};
    };
    const auto multiply = [](const Quatd& left, const Quatd& right) {
        return Quatd{
            (left.w * right.x) + (left.x * right.w) + (left.y * right.z) - (left.z * right.y),
            (left.w * right.y) - (left.x * right.z) + (left.y * right.w) + (left.z * right.x),
            (left.w * right.z) + (left.x * right.y) - (left.y * right.x) + (left.z * right.w),
            (left.w * right.w) - (left.x * right.x) - (left.y * right.y) - (left.z * right.z),
        };
    };
    const auto normalize = [](const Quatd& value) {
        const double length = std::sqrt((value.x * value.x) + (value.y * value.y) +
                                        (value.z * value.z) + (value.w * value.w));
        if (length <= 0.0) {
            return Quatd{};
        }
        return Quatd{value.x / length, value.y / length, value.z / length, value.w / length};
    };
    const auto cross = [](const core::Vec3d& left, const core::Vec3d& right) {
        return core::Vec3d{
            (left.y * right.z) - (left.z * right.y),
            (left.z * right.x) - (left.x * right.z),
            (left.x * right.y) - (left.y * right.x),
        };
    };
    const auto rotate = [&cross](const Quatd& rotation, const core::Vec3d& value) {
        const core::Vec3d q{rotation.x, rotation.y, rotation.z};
        const core::Vec3d uv = cross(q, value);
        const core::Vec3d uuv = cross(q, uv);
        return core::Vec3d{
            value.x + (2.0 * ((uv.x * rotation.w) + uuv.x)),
            value.y + (2.0 * ((uv.y * rotation.w) + uuv.y)),
            value.z + (2.0 * ((uv.z * rotation.w) + uuv.z)),
        };
    };

    std::vector<WorldTransform> world(asset.nodes.size());
    std::vector<std::uint8_t> computed(asset.nodes.size(), 0);
    const auto compute = [&](const auto& self, std::size_t index) -> void {
        if (computed[index] != 0) {
            return;
        }
        const Node& node = asset.nodes[index];
        const Quatd local_rotation{
            static_cast<double>(node.rotation.x),
            static_cast<double>(node.rotation.y),
            static_cast<double>(node.rotation.z),
            static_cast<double>(node.rotation.w),
        };
        const core::Vec3d local_scale{
            static_cast<double>(node.scale.x),
            static_cast<double>(node.scale.y),
            static_cast<double>(node.scale.z),
        };
        if (!node.parent.has_value()) {
            world[index].position = node.translation;
            world[index].rotation = node.rotation;
            world[index].scale = node.scale;
        } else {
            self(self, *node.parent);
            const WorldTransform& parent = world[*node.parent];
            const Quatd parent_rotation{
                static_cast<double>(parent.rotation.x),
                static_cast<double>(parent.rotation.y),
                static_cast<double>(parent.rotation.z),
                static_cast<double>(parent.rotation.w),
            };
            const core::Vec3d parent_scale{
                static_cast<double>(parent.scale.x),
                static_cast<double>(parent.scale.y),
                static_cast<double>(parent.scale.z),
            };
            const core::Vec3d scaled_translation{
                node.translation.x * static_cast<double>(parent_scale.x),
                node.translation.y * static_cast<double>(parent_scale.y),
                node.translation.z * static_cast<double>(parent_scale.z),
            };
            const core::Vec3d rotated = rotate(parent_rotation, scaled_translation);
            world[index].position = core::Vec3d{
                parent.position.x + rotated.x,
                parent.position.y + rotated.y,
                parent.position.z + rotated.z,
            };
            const Quatd combined = normalize(multiply(parent_rotation, local_rotation));
            world[index].rotation = core::Quat{
                static_cast<float>(combined.x),
                static_cast<float>(combined.y),
                static_cast<float>(combined.z),
                static_cast<float>(combined.w),
            };
            world[index].scale = core::Vec3{
                static_cast<float>(static_cast<double>(parent.scale.x) * local_scale.x),
                static_cast<float>(static_cast<double>(parent.scale.y) * local_scale.y),
                static_cast<float>(static_cast<double>(parent.scale.z) * local_scale.z),
            };
        }
        computed[index] = 1;
    };
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        compute(compute, index);
    }
    return world;
}

Result<ImportedAsset> import_glb(std::span<const std::byte> bytes, std::string source_name) {
    if (bytes.size() > kMaxSourceBytes) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Validation, "GLB exceeds the source size limit", "",
                              {{"bytes", bytes.size()}, {"limit", kMaxSourceBytes}}));
    }
    if (bytes.size() < 12U || bytes[0] != std::byte{'g'} || bytes[1] != std::byte{'l'} ||
        bytes[2] != std::byte{'T'} || bytes[3] != std::byte{'F'}) {
        const bool json = !bytes.empty() && bytes[0] == std::byte{'{'};
        return core::make_failure<ImportedAsset>(make_import_error(
            json ? ErrorCode::Unsupported : ErrorCode::Validation,
            json ? "glTF JSON containers are not supported; export a self-contained .glb"
                 : "file is not a binary glTF container",
            "", {{"source", source_name}}));
    }

    auto buffer = fastgltf::GltfDataBuffer::FromBytes(bytes.data(), bytes.size());
    if (!buffer) {
        return core::make_failure<ImportedAsset>(from_fastgltf(buffer.error(), source_name));
    }
    fastgltf::Parser parser{fastgltf::Extensions::None};
    const auto options =
        fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::GenerateMeshIndices;
    auto parsed = parser.loadGltfBinary(buffer.get(), {}, options, fastgltf::Category::All);
    if (parsed.error() != fastgltf::Error::None) {
        return core::make_failure<ImportedAsset>(from_fastgltf(parsed.error(), source_name));
    }
    const fastgltf::Asset& gltf = parsed.get();
    if (auto animations = validate_animation_references(gltf); !animations) {
        return core::make_failure<ImportedAsset>(animations.error());
    }

    if (!gltf.extensionsRequired.empty()) {
        nlohmann::json extensions = nlohmann::json::array();
        for (const auto& extension : gltf.extensionsRequired) {
            extensions.push_back(extension);
        }
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Unsupported, "asset requires unsupported glTF extensions",
                              "/extensionsRequired", {{"extensions", std::move(extensions)}}));
    }
    if (!gltf.extensionsUsed.empty()) {
        nlohmann::json extensions = nlohmann::json::array();
        for (const auto& extension : gltf.extensionsUsed) {
            extensions.push_back(extension);
        }
        return core::make_failure<ImportedAsset>(make_import_error(
            ErrorCode::Unsupported, "asset uses glTF extensions that the M3 importer ignores",
            "/extensionsUsed", {{"extensions", std::move(extensions)}}));
    }
    if (!gltf.skins.empty()) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Unsupported, "skinned meshes are not supported in M3",
                              "/skins", {{"skins", gltf.skins.size()}}));
    }
    if (!gltf.cameras.empty()) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Unsupported, "cameras are not imported from assets in M3",
                              "/cameras", {{"cameras", gltf.cameras.size()}}));
    }
    if (gltf.nodes.size() > kMaxNodes) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Validation, "asset exceeds the node limit", "/nodes",
                              {{"nodes", gltf.nodes.size()}, {"limit", kMaxNodes}}));
    }
    if (gltf.meshes.size() > kMaxMeshes) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Validation, "asset exceeds the mesh limit", "/meshes",
                              {{"meshes", gltf.meshes.size()}, {"limit", kMaxMeshes}}));
    }

    ImportedAsset asset;
    asset.source = std::move(source_name);
    asset.generator = gltf.assetInfo.has_value() ? gltf.assetInfo->generator : "";
    asset.animation_channel_count = 0;
    for (const auto& animation : gltf.animations) {
        asset.animation_channel_count += animation.channels.size();
    }

    // Buffers: only embedded GLB buffers are supported in M3. Verify every
    // buffer, buffer view and accessor against the actual data before any
    // accessor is read; fastgltf checks the declared lengths, not the bytes
    // present in the GLB.
    if (auto layout = validate_buffer_layout(gltf); !layout) {
        return core::make_failure<ImportedAsset>(layout.error());
    }

    for (std::size_t index = 0; index < gltf.images.size(); ++index) {
        auto bytes_value = image_bytes(gltf, index);
        if (!bytes_value) {
            return core::make_failure<ImportedAsset>(bytes_value.error());
        }
        if (bytes_value->size() > kMaxImageBytes) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Validation, "image exceeds the encoded-size limit",
                "/images/" + std::to_string(index),
                {{"image", index}, {"bytes", bytes_value->size()}, {"limit", kMaxImageBytes}}));
        }
        fastgltf::MimeType mime = fastgltf::MimeType::None;
        std::visit(
            [&mime](const auto& data) {
                using T = std::decay_t<decltype(data)>;
                if constexpr (std::is_same_v<T, fastgltf::sources::BufferView> ||
                              std::is_same_v<T, fastgltf::sources::Array> ||
                              std::is_same_v<T, fastgltf::sources::ByteView> ||
                              std::is_same_v<T, fastgltf::sources::Vector>) {
                    mime = data.mimeType;
                }
            },
            gltf.images[index].data);
        const auto converted = image_mime_type(mime);
        if (!converted.has_value()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "only PNG and JPEG images are supported in M3",
                "/images/" + std::to_string(index),
                {{"image", index}, {"mimeType", static_cast<int>(mime)}}));
        }
        asset.images.push_back(
            Image{std::string(gltf.images[index].name), *converted, std::move(*bytes_value)});
    }

    for (const auto& source : gltf.samplers) {
        Sampler sampler;
        if (source.magFilter.has_value()) {
            sampler.mag_filter = static_cast<Filter>(*source.magFilter);
        }
        if (source.minFilter.has_value()) {
            sampler.min_filter = static_cast<Filter>(*source.minFilter);
        }
        sampler.wrap_s = static_cast<Wrap>(source.wrapS);
        sampler.wrap_t = static_cast<Wrap>(source.wrapT);
        asset.samplers.push_back(sampler);
    }

    for (std::size_t index = 0; index < gltf.textures.size(); ++index) {
        const auto& source = gltf.textures[index];
        if (!source.imageIndex.has_value()) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Unsupported, "texture has no core image source",
                                  "/textures/" + std::to_string(index), {{"texture", index}}));
        }
        if (*source.imageIndex >= gltf.images.size()) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Validation, "texture image index is out of range",
                                  "/textures/" + std::to_string(index),
                                  {{"texture", index}, {"image", *source.imageIndex}}));
        }
        if (source.samplerIndex.has_value() && *source.samplerIndex >= gltf.samplers.size()) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Validation, "texture sampler index is out of range",
                                  "/textures/" + std::to_string(index),
                                  {{"texture", index}, {"sampler", *source.samplerIndex}}));
        }
        asset.textures.push_back(
            Texture{std::string(source.name), *source.imageIndex, source.samplerIndex});
    }

    for (std::size_t index = 0; index < gltf.materials.size(); ++index) {
        auto material = convert_material(gltf.materials[index], gltf, index);
        if (!material) {
            return core::make_failure<ImportedAsset>(material.error());
        }
        asset.materials.push_back(std::move(*material));
    }

    asset.meshes.reserve(gltf.meshes.size());
    std::size_t total_primitives = 0;
    for (std::size_t mesh_index = 0; mesh_index < gltf.meshes.size(); ++mesh_index) {
        const auto& source = gltf.meshes[mesh_index];
        if (!source.weights.empty()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "morph target weights are not supported in M3",
                "/meshes/" + std::to_string(mesh_index) + "/weights", {{"mesh", mesh_index}}));
        }
        Mesh mesh;
        mesh.name =
            source.name.empty() ? "mesh_" + std::to_string(mesh_index) : std::string(source.name);
        mesh.primitives.reserve(source.primitives.size());
        for (std::size_t primitive_index = 0; primitive_index < source.primitives.size();
             ++primitive_index) {
            const auto& source_primitive = source.primitives[primitive_index];
            const std::string primitive_pointer = "/meshes/" + std::to_string(mesh_index) +
                                                  "/primitives/" + std::to_string(primitive_index);
            if (source_primitive.type != fastgltf::PrimitiveType::Triangles) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Unsupported, "only triangle primitives are supported",
                    primitive_pointer, {{"mesh", mesh_index}, {"primitive", primitive_index}}));
            }
            if (!source_primitive.targets.empty()) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Unsupported, "morph targets are not supported in M3",
                    primitive_pointer + "/targets",
                    {{"mesh", mesh_index}, {"primitive", primitive_index}}));
            }
            if (!source_primitive.mappings.empty()) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Unsupported, "material variants are not supported in M3",
                    primitive_pointer, {{"mesh", mesh_index}, {"primitive", primitive_index}}));
            }
            if (source_primitive.dracoCompression != nullptr) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Unsupported, "Draco-compressed primitives are not supported",
                    "/meshes", {{"mesh", mesh_index}, {"primitive", primitive_index}}));
            }
            if (++total_primitives > kMaxPrimitives) {
                return core::make_failure<ImportedAsset>(
                    make_import_error(ErrorCode::Validation, "asset exceeds the primitive limit",
                                      "/meshes", {{"limit", kMaxPrimitives}}));
            }
            auto vertices = read_vertices(gltf, source_primitive, mesh_index, primitive_index);
            if (!vertices) {
                return core::make_failure<ImportedAsset>(vertices.error());
            }
            auto indices =
                read_indices(gltf, source_primitive, mesh_index, primitive_index, vertices->size());
            if (!indices) {
                return core::make_failure<ImportedAsset>(indices.error());
            }
            std::optional<std::size_t> material;
            if (source_primitive.materialIndex.has_value()) {
                if (*source_primitive.materialIndex >= gltf.materials.size()) {
                    return core::make_failure<ImportedAsset>(make_import_error(
                        ErrorCode::Validation, "primitive material index is out of range",
                        "/meshes", {{"mesh", mesh_index}, {"primitive", primitive_index}}));
                }
                material = *source_primitive.materialIndex;
            }
            Primitive primitive;
            primitive.material = material;
            primitive.bounds = compute_bounds(*vertices);
            primitive.vertices = std::move(*vertices);
            primitive.indices = std::move(*indices);
            mesh.primitives.push_back(std::move(primitive));
        }
        asset.meshes.push_back(std::move(mesh));
    }

    // Parent links and cycle detection.
    std::vector<std::optional<std::size_t>> parents(gltf.nodes.size());
    for (std::size_t index = 0; index < gltf.nodes.size(); ++index) {
        for (const std::size_t child : gltf.nodes[index].children) {
            if (child >= gltf.nodes.size()) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Validation, "node child index is out of range",
                    "/nodes/" + std::to_string(index), {{"node", index}, {"child", child}}));
            }
            if (parents[child].has_value()) {
                return core::make_failure<ImportedAsset>(make_import_error(
                    ErrorCode::Validation, "node has more than one parent",
                    "/nodes/" + std::to_string(child), {{"node", child}, {"parent", index}}));
            }
            parents[child] = index;
        }
    }
    std::vector<std::uint8_t> state(gltf.nodes.size(), 0);
    const auto visit = [&](const auto& self, std::size_t index, std::size_t depth) -> Result<void> {
        if (depth > kMaxNodeDepth) {
            return core::make_failure<void>(make_import_error(
                ErrorCode::Validation, "node hierarchy exceeds the depth limit",
                "/nodes/" + std::to_string(index), {{"node", index}, {"limit", kMaxNodeDepth}}));
        }
        if (state[index] == 1) {
            return core::make_failure<void>(
                make_import_error(ErrorCode::Validation, "node hierarchy contains a cycle",
                                  "/nodes/" + std::to_string(index), {{"node", index}}));
        }
        if (state[index] == 2) {
            return {};
        }
        state[index] = 1;
        for (const std::size_t child : gltf.nodes[index].children) {
            if (auto result = self(self, child, depth + 1U); !result) {
                return result;
            }
        }
        state[index] = 2;
        return {};
    };
    for (std::size_t index = 0; index < gltf.nodes.size(); ++index) {
        if (auto result = visit(visit, index, 0U); !result) {
            return core::make_failure<ImportedAsset>(result.error());
        }
    }

    if (gltf.scenes.size() > 1U) {
        return core::make_failure<ImportedAsset>(make_import_error(
            ErrorCode::Unsupported, "assets with more than one scene are not supported", "/scenes",
            {{"scenes", gltf.scenes.size()}}));
    }
    if (!gltf.scenes.empty()) {
        const std::size_t scene_index = gltf.defaultScene.value_or(0);
        if (scene_index >= gltf.scenes.size()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Validation, "default scene index is out of range", "/scenes",
                {{"scene", scene_index}, {"count", gltf.scenes.size()}}));
        }
        asset.scene_roots.assign(gltf.scenes[scene_index].nodeIndices.begin(),
                                 gltf.scenes[scene_index].nodeIndices.end());
        asset.default_scene = scene_index;
        asset.scene_name = gltf.scenes[scene_index].name;
    } else {
        for (std::size_t index = 0; index < parents.size(); ++index) {
            if (!parents[index].has_value()) {
                asset.scene_roots.push_back(index);
            }
        }
    }
    for (const std::size_t root : asset.scene_roots) {
        if (root >= gltf.nodes.size()) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Validation, "scene root index is out of range",
                                  "/scenes", {{"root", root}}));
        }
    }

    asset.nodes.reserve(gltf.nodes.size());
    for (std::size_t index = 0; index < gltf.nodes.size(); ++index) {
        const auto& source = gltf.nodes[index];
        const auto* trs = std::get_if<fastgltf::TRS>(&source.transform);
        if (trs == nullptr) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "node matrix could not be decomposed to TRS",
                "/nodes/" + std::to_string(index), {{"node", index}}));
        }
        if (source.meshIndex.has_value() && *source.meshIndex >= gltf.meshes.size()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Validation, "node mesh index is out of range",
                "/nodes/" + std::to_string(index), {{"node", index}, {"mesh", *source.meshIndex}}));
        }
        if (source.skinIndex.has_value()) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Unsupported, "skinned nodes are not supported in M3",
                                  "/nodes/" + std::to_string(index), {{"node", index}}));
        }
        if (source.cameraIndex.has_value()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "cameras are not imported from assets in M3",
                "/nodes/" + std::to_string(index) + "/camera", {{"node", index}}));
        }
        if (!source.weights.empty()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "morph target weights are not supported in M3",
                "/nodes/" + std::to_string(index) + "/weights", {{"node", index}}));
        }
        if (!source.instancingAttributes.empty()) {
            return core::make_failure<ImportedAsset>(make_import_error(
                ErrorCode::Unsupported, "GPU instancing is not supported in M3",
                "/nodes/" + std::to_string(index) + "/extensions", {{"node", index}}));
        }
        const auto finite = [](float value) { return std::isfinite(value); };
        const float rotation_length = std::sqrt(
            (trs->rotation[0] * trs->rotation[0]) + (trs->rotation[1] * trs->rotation[1]) +
            (trs->rotation[2] * trs->rotation[2]) + (trs->rotation[3] * trs->rotation[3]));
        if (!finite(trs->translation[0]) || !finite(trs->translation[1]) ||
            !finite(trs->translation[2]) || !finite(trs->rotation[0]) ||
            !finite(trs->rotation[1]) || !finite(trs->rotation[2]) || !finite(trs->rotation[3]) ||
            !finite(trs->scale[0]) || !finite(trs->scale[1]) || !finite(trs->scale[2]) ||
            std::abs(rotation_length - 1.0F) > 1.0e-3F) {
            return core::make_failure<ImportedAsset>(
                make_import_error(ErrorCode::Validation,
                                  "node transform is not finite or rotation is not unit length",
                                  "/nodes/" + std::to_string(index), {{"node", index}}));
        }
        Node node;
        node.source_index = index;
        node.name =
            source.name.empty() ? "node_" + std::to_string(index) : std::string(source.name);
        node.parent = parents[index];
        node.mesh = source.meshIndex;
        node.translation = core::Vec3d{
            static_cast<double>(trs->translation[0]),
            static_cast<double>(trs->translation[1]),
            static_cast<double>(trs->translation[2]),
        };
        node.rotation =
            core::Quat{trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]};
        node.scale = core::Vec3{trs->scale[0], trs->scale[1], trs->scale[2]};
        node.children.assign(source.children.begin(), source.children.end());
        asset.nodes.push_back(std::move(node));
    }

    // Build stable path keys after all parents are known.
    std::vector<std::string> paths(asset.nodes.size());
    const auto path = [&](const auto& self, std::size_t index) -> std::string {
        if (!paths[index].empty()) {
            return paths[index];
        }
        const Node& node = asset.nodes[index];
        if (!node.parent.has_value()) {
            paths[index] = node.name;
        } else {
            paths[index] = self(self, *node.parent) + "/" + node.name;
        }
        return paths[index];
    };
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        asset.nodes[index].path = path(path, index);
    }

    return asset;
}

Result<ImportedAsset> import_glb_file(const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        return core::make_failure<ImportedAsset>(
            core::make_error(ErrorCode::Io, "cannot read asset '" + path.generic_string() + "'",
                             filesystem_error.message()));
    }
    if (size > kMaxSourceBytes) {
        return core::make_failure<ImportedAsset>(
            make_import_error(ErrorCode::Validation, "GLB exceeds the source size limit", "",
                              {{"bytes", size}, {"limit", kMaxSourceBytes}}));
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return core::make_failure<ImportedAsset>(
            core::make_error(ErrorCode::Io, "cannot open asset '" + path.generic_string() + "'"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty()) {
        return core::make_failure<ImportedAsset>(
            core::make_error(ErrorCode::Io, "cannot read asset '" + path.generic_string() + "'"));
    }
    return import_glb(bytes, path.generic_string());
}

} // namespace engine::asset
