#include <engine/render/offscreen_renderer.hpp>

#include <engine/render/camera.hpp>
#include <engine/render/frustum.hpp>
#include <engine/render/scene_transform.hpp>

#include "shaders.hpp"
#include "vulkan_util.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace engine::render {
namespace {

constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    float radius{0.0F};
};

struct PushConstants {
    glm::mat4 view_projection{1.0F};
    glm::mat4 model{1.0F};
};

[[nodiscard]] MeshData make_cube_mesh() {
    constexpr std::array<glm::vec3, 8> positions{
        glm::vec3{-0.5F, -0.5F, -0.5F}, glm::vec3{0.5F, -0.5F, -0.5F}, glm::vec3{0.5F, 0.5F, -0.5F},
        glm::vec3{-0.5F, 0.5F, -0.5F},  glm::vec3{-0.5F, -0.5F, 0.5F}, glm::vec3{0.5F, -0.5F, 0.5F},
        glm::vec3{0.5F, 0.5F, 0.5F},    glm::vec3{-0.5F, 0.5F, 0.5F},
    };
    constexpr std::array<glm::vec3, 8> colors{
        glm::vec3{0.9F, 0.2F, 0.2F}, glm::vec3{0.2F, 0.9F, 0.2F}, glm::vec3{0.2F, 0.3F, 0.9F},
        glm::vec3{0.9F, 0.9F, 0.2F}, glm::vec3{0.9F, 0.2F, 0.9F}, glm::vec3{0.2F, 0.9F, 0.9F},
        glm::vec3{0.9F, 0.6F, 0.2F}, glm::vec3{0.6F, 0.2F, 0.9F},
    };

    MeshData mesh;
    mesh.vertices.reserve(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        mesh.vertices.push_back({positions[index], colors[index]});
    }
    mesh.indices = {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2, 3, 2, 6, 3, 6, 7, 0, 4, 5, 0, 5, 1,
    };
    mesh.radius = 0.8660254F;
    return mesh;
}

[[nodiscard]] MeshData make_plane_mesh() {
    MeshData mesh;
    mesh.vertices = {
        {glm::vec3{-5.0F, 0.0F, -5.0F}, glm::vec3{0.35F, 0.4F, 0.35F}},
        {glm::vec3{5.0F, 0.0F, -5.0F}, glm::vec3{0.35F, 0.45F, 0.35F}},
        {glm::vec3{5.0F, 0.0F, 5.0F}, glm::vec3{0.4F, 0.5F, 0.4F}},
        {glm::vec3{-5.0F, 0.0F, 5.0F}, glm::vec3{0.4F, 0.5F, 0.4F}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.radius = 7.071068F;
    return mesh;
}

[[nodiscard]] core::Result<VkShaderModule>
create_shader_module(VkDevice device, const std::uint32_t* code, std::size_t size) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &module);
    if (result != VK_SUCCESS) {
        return core::make_failure<VkShaderModule>(
            detail::vulkan_error("could not create a shader module", "shader", result));
    }
    return module;
}

void transition_image(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
                      VkImageLayout new_layout, VkPipelineStageFlags2 source_stage,
                      VkAccessFlags2 source_access, VkPipelineStageFlags2 destination_stage,
                      VkAccessFlags2 destination_access, VkImageAspectFlags aspect) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = source_stage;
    barrier.srcAccessMask = source_access;
    barrier.dstStageMask = destination_stage;
    barrier.dstAccessMask = destination_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

} // namespace

struct OffscreenRenderer::Impl {
    struct GpuMesh {
        VkBuffer vertex_buffer{VK_NULL_HANDLE};
        VmaAllocation vertex_allocation{nullptr};
        VkBuffer index_buffer{VK_NULL_HANDLE};
        VmaAllocation index_allocation{nullptr};
        std::uint32_t index_count{0};
        float radius{0.0F};
    };

    const Device* device{nullptr};
    std::uint32_t width{0};
    std::uint32_t height{0};
    VkImage color_image{VK_NULL_HANDLE};
    VmaAllocation color_allocation{nullptr};
    VkImageView color_view{VK_NULL_HANDLE};
    VkImage depth_image{VK_NULL_HANDLE};
    VmaAllocation depth_allocation{nullptr};
    VkImageView depth_view{VK_NULL_HANDLE};
    VkCommandPool command_pool{VK_NULL_HANDLE};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkFence fence{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    GpuMesh cube_mesh;
    GpuMesh plane_mesh;
    VkBuffer readback_buffer{VK_NULL_HANDLE};
    VmaAllocation readback_allocation{nullptr};

    ~Impl();

    [[nodiscard]] core::Result<void> initialize(const Device& render_device,
                                                const OffscreenOptions& options);
    [[nodiscard]] core::Result<void> create_targets();
    void destroy_targets();
    [[nodiscard]] core::Result<void> create_command_resources();
    void destroy_command_resources();
    [[nodiscard]] core::Result<GpuMesh> create_gpu_mesh(const MeshData& mesh);
    void destroy_gpu_mesh(GpuMesh& mesh);
    [[nodiscard]] core::Result<void> create_builtin_meshes();
    void destroy_builtin_meshes();
    [[nodiscard]] const GpuMesh* find_mesh(std::string_view mesh_id) const noexcept;
    [[nodiscard]] core::Result<void> create_pipeline();
    [[nodiscard]] core::Result<std::vector<std::uint8_t>> draw(const scene::SceneView& view,
                                                               int frames);
};

OffscreenRenderer::Impl::~Impl() {
    if (device != nullptr && device->device() != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device->device());
    }
    if (device == nullptr) {
        return;
    }
    const VkDevice handle = device->device();
    if (readback_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device->allocator(), readback_buffer, readback_allocation);
    }
    destroy_builtin_meshes();
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(handle, pipeline, nullptr);
    }
    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(handle, pipeline_layout, nullptr);
    }
    destroy_command_resources();
    destroy_targets();
}

core::Result<void> OffscreenRenderer::Impl::initialize(const Device& render_device,
                                                       const OffscreenOptions& options) {
    if (options.width == 0 || options.height == 0) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Validation, "offscreen size must be positive", {},
                             {{"width", options.width}, {"height", options.height}}));
    }
    device = &render_device;
    width = options.width;
    height = options.height;

    auto targets = create_targets();
    if (!targets) {
        return targets;
    }
    auto commands = create_command_resources();
    if (!commands) {
        return commands;
    }
    auto meshes = create_builtin_meshes();
    if (!meshes) {
        return meshes;
    }
    return create_pipeline();
}

core::Result<void> OffscreenRenderer::Impl::create_targets() {
    const VkExtent3D extent{width, height, 1};
    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent = extent;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    image_info.format = kColorFormat;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkResult result = vmaCreateImage(device->allocator(), &image_info, &allocation_info,
                                     &color_image, &color_allocation, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen color image", "offscreen", result));
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = color_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = kColorFormat;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    result = vkCreateImageView(device->device(), &view_info, nullptr, &color_view);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the offscreen color view", "offscreen", result));
    }

    image_info.format = kDepthFormat;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    result = vmaCreateImage(device->allocator(), &image_info, &allocation_info, &depth_image,
                            &depth_allocation, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen depth image", "offscreen", result));
    }
    view_info.image = depth_image;
    view_info.format = kDepthFormat;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    result = vkCreateImageView(device->device(), &view_info, nullptr, &depth_view);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the offscreen depth view", "offscreen", result));
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = static_cast<VkDeviceSize>(width) * height * 4U;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo readback_allocation_info{};
    readback_allocation_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    result = vmaCreateBuffer(device->allocator(), &buffer_info, &readback_allocation_info,
                             &readback_buffer, &readback_allocation, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen readback buffer", "offscreen", result));
    }
    return {};
}

void OffscreenRenderer::Impl::destroy_targets() {
    if (device == nullptr) {
        return;
    }
    if (color_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device->device(), color_view, nullptr);
        color_view = VK_NULL_HANDLE;
    }
    if (color_image != VK_NULL_HANDLE) {
        vmaDestroyImage(device->allocator(), color_image, color_allocation);
        color_image = VK_NULL_HANDLE;
        color_allocation = nullptr;
    }
    if (depth_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device->device(), depth_view, nullptr);
        depth_view = VK_NULL_HANDLE;
    }
    if (depth_image != VK_NULL_HANDLE) {
        vmaDestroyImage(device->allocator(), depth_image, depth_allocation);
        depth_image = VK_NULL_HANDLE;
        depth_allocation = nullptr;
    }
}

core::Result<void> OffscreenRenderer::Impl::create_command_resources() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = device->queue_families().graphics;
    VkResult result = vkCreateCommandPool(device->device(), &pool_info, nullptr, &command_pool);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen command pool", "offscreen", result));
    }

    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device->device(), &allocate_info, &command_buffer);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not allocate the offscreen command buffer", "offscreen", result));
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = vkCreateFence(device->device(), &fence_info, nullptr, &fence);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the offscreen fence", "offscreen", result));
    }
    return {};
}

void OffscreenRenderer::Impl::destroy_command_resources() {
    if (device == nullptr) {
        return;
    }
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(device->device(), fence, nullptr);
        fence = VK_NULL_HANDLE;
    }
    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device->device(), command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
        command_buffer = VK_NULL_HANDLE;
    }
}

core::Result<OffscreenRenderer::Impl::GpuMesh>
OffscreenRenderer::Impl::create_gpu_mesh(const MeshData& mesh) {
    const auto create_buffer = [&](VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer,
                                   VmaAllocation& allocation) -> core::Result<void> {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        const VkResult result = vmaCreateBuffer(device->allocator(), &buffer_info, &allocation_info,
                                                &buffer, &allocation, nullptr);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(detail::vulkan_error(
                "could not create an offscreen mesh buffer", "offscreen", result));
        }
        return {};
    };

    GpuMesh gpu;
    auto vertex =
        create_buffer(mesh.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      gpu.vertex_buffer, gpu.vertex_allocation);
    if (!vertex) {
        return core::make_failure<GpuMesh>(vertex.error());
    }
    auto index =
        create_buffer(mesh.indices.size() * sizeof(std::uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      gpu.index_buffer, gpu.index_allocation);
    if (!index) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(index.error());
    }

    void* mapped = nullptr;
    VkResult result = vmaMapMemory(device->allocator(), gpu.vertex_allocation, &mapped);
    if (result != VK_SUCCESS) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(
            detail::vulkan_error("could not map an offscreen vertex buffer", "offscreen", result));
    }
    std::memcpy(mapped, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(device->allocator(), gpu.vertex_allocation);
    result = vmaMapMemory(device->allocator(), gpu.index_allocation, &mapped);
    if (result != VK_SUCCESS) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(
            detail::vulkan_error("could not map an offscreen index buffer", "offscreen", result));
    }
    std::memcpy(mapped, mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    vmaUnmapMemory(device->allocator(), gpu.index_allocation);
    gpu.index_count = static_cast<std::uint32_t>(mesh.indices.size());
    gpu.radius = mesh.radius;
    return gpu;
}

// clang-tidy cannot see the resource writes through the reference.
// NOLINTNEXTLINE(readability-make-member-function-const)
void OffscreenRenderer::Impl::destroy_gpu_mesh(GpuMesh& mesh) {
    if (device == nullptr) {
        return;
    }
    if (mesh.index_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device->allocator(), mesh.index_buffer, mesh.index_allocation);
        mesh.index_buffer = VK_NULL_HANDLE;
        mesh.index_allocation = nullptr;
    }
    if (mesh.vertex_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device->allocator(), mesh.vertex_buffer, mesh.vertex_allocation);
        mesh.vertex_buffer = VK_NULL_HANDLE;
        mesh.vertex_allocation = nullptr;
    }
}

core::Result<void> OffscreenRenderer::Impl::create_builtin_meshes() {
    auto cube = create_gpu_mesh(make_cube_mesh());
    if (!cube) {
        return core::make_failure<void>(cube.error());
    }
    cube_mesh = *cube;
    auto plane = create_gpu_mesh(make_plane_mesh());
    if (!plane) {
        destroy_gpu_mesh(cube_mesh);
        return core::make_failure<void>(plane.error());
    }
    plane_mesh = *plane;
    return {};
}

void OffscreenRenderer::Impl::destroy_builtin_meshes() {
    destroy_gpu_mesh(cube_mesh);
    destroy_gpu_mesh(plane_mesh);
}

const OffscreenRenderer::Impl::GpuMesh*
OffscreenRenderer::Impl::find_mesh(std::string_view mesh_id) const noexcept {
    if (mesh_id == scene::kBuiltinCubeMesh) {
        return &cube_mesh;
    }
    if (mesh_id == scene::kBuiltinPlaneMesh) {
        return &plane_mesh;
    }
    return nullptr;
}

core::Result<void> OffscreenRenderer::Impl::create_pipeline() {
    auto vertex_module = create_shader_module(device->device(), shaders::kTriangleVertex,
                                              shaders::kTriangleVertexSize);
    if (!vertex_module) {
        return core::make_failure<void>(vertex_module.error());
    }
    auto fragment_module = create_shader_module(device->device(), shaders::kTriangleFragment,
                                                shaders::kTriangleFragmentSize);
    if (!fragment_module) {
        vkDestroyShaderModule(device->device(), *vertex_module, nullptr);
        return core::make_failure<void>(fragment_module.error());
    }

    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant.size = sizeof(PushConstants);
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;
    VkResult result =
        vkCreatePipelineLayout(device->device(), &layout_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device->device(), *vertex_module, nullptr);
        vkDestroyShaderModule(device->device(), *fragment_module, nullptr);
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen pipeline layout", "offscreen", result));
    }

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = *vertex_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = *fragment_module;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = static_cast<std::uint32_t>(sizeof(glm::vec3));

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0F;

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_G_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_B_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_attachment;

    const std::array<VkDynamicState, 2> dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &kColorFormat;
    rendering.depthAttachmentFormat = kDepthFormat;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering;
    pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;

    VkPipeline new_pipeline = VK_NULL_HANDLE;
    result = vkCreateGraphicsPipelines(device->device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                       &new_pipeline);
    vkDestroyShaderModule(device->device(), *vertex_module, nullptr);
    vkDestroyShaderModule(device->device(), *fragment_module, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(
            "could not create the offscreen graphics pipeline", "offscreen", result));
    }
    pipeline = new_pipeline;
    return {};
}

core::Result<std::vector<std::uint8_t>> OffscreenRenderer::Impl::draw(const scene::SceneView& view,
                                                                      int frames) {
    const auto camera_entity = view.active_camera();
    if (!camera_entity.has_value()) {
        return core::make_failure<std::vector<std::uint8_t>>(core::make_error(
            core::ErrorCode::NotFound, "the scene has no active camera", {}, {{"stage", "scene"}}));
    }
    for (const scene::Entity entity : view.entities()) {
        if (view.has_mesh(entity) && find_mesh(view.mesh(entity).mesh_id) == nullptr) {
            return core::make_failure<std::vector<std::uint8_t>>(core::make_error(
                core::ErrorCode::Unsupported, "the scene references an unknown mesh", {},
                {{"stage", "mesh"}, {"meshId", view.mesh(entity).mesh_id}}));
        }
    }
    if (frames <= 0) {
        return core::make_failure<std::vector<std::uint8_t>>(core::make_error(
            core::ErrorCode::Validation, "frames must be positive", {}, {{"frames", frames}}));
    }

    const scene::WorldTransform camera_world = view.world_transform(*camera_entity);
    const scene::Camera& camera_component = view.camera(*camera_entity);
    Camera camera;
    camera.vertical_fov_radians = camera_component.fovYRadians;
    camera.near_plane = camera_component.near;
    camera.far_plane = camera_component.far;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 view_projection =
        camera.projection(aspect) * camera_view_from_world(camera_world);
    const Frustum frustum = Frustum::from_view_projection(view_projection);

    for (int frame = 0; frame < frames; ++frame) {
        VkResult result = vkWaitForFences(device->device(), 1, &fence, VK_TRUE,
                                          std::numeric_limits<std::uint64_t>::max());
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
                "could not wait for the offscreen fence", "offscreen", result));
        }
        result = vkResetFences(device->device(), 1, &fence);
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(
                detail::vulkan_error("could not reset the offscreen fence", "offscreen", result));
        }
        result = vkResetCommandBuffer(command_buffer, 0);
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
                "could not reset the offscreen command buffer", "offscreen", result));
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(command_buffer, &begin_info);
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
                "could not begin the offscreen command buffer", "offscreen", result));
        }

        transition_image(command_buffer, color_image, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        constexpr VkPipelineStageFlags2 kDepthStages =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        transition_image(command_buffer, depth_image, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, kDepthStages, 0, kDepthStages,
                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue.color = {{0.04F, 0.05F, 0.07F, 1.0F}};

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue.depthStencil.depth = 1.0F;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.extent = VkExtent2D{width, height};
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.pDepthAttachment = &depth_attachment;
        vkCmdBeginRendering(command_buffer, &rendering_info);

        VkViewport viewport{};
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.maxDepth = 1.0F;
        VkRect2D scissor{};
        scissor.extent = VkExtent2D{width, height};
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        PushConstants push_constants;
        push_constants.view_projection = view_projection;
        for (const scene::Entity entity : view.entities()) {
            if (!view.has_mesh(entity)) {
                continue;
            }
            const GpuMesh* mesh = find_mesh(view.mesh(entity).mesh_id);
            if (mesh == nullptr) {
                continue;
            }
            const scene::WorldTransform world = view.world_transform(entity);
            const glm::mat4 model = camera_relative_model(world, camera_world.position);
            const float scale = std::max({
                static_cast<float>(world.scale.x),
                static_cast<float>(world.scale.y),
                static_cast<float>(world.scale.z),
            });
            const glm::vec3 center{model[3][0], model[3][1], model[3][2]};
            if (!frustum.intersects_sphere(center, mesh->radius * scale)) {
                continue;
            }
            push_constants.model = model;
            vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(PushConstants), &push_constants);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &mesh->vertex_buffer, &offset);
            vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, 0, 0, 0);
        }
        vkCmdEndRendering(command_buffer);

        transition_image(command_buffer, color_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = VkExtent3D{width, height, 1};
        vkCmdCopyImageToBuffer(command_buffer, color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback_buffer, 1, &copy);
        result = vkEndCommandBuffer(command_buffer);
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
                "could not end the offscreen command buffer", "offscreen", result));
        }

        VkCommandBufferSubmitInfo command_info{};
        command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_info.commandBuffer = command_buffer;
        VkSubmitInfo2 submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &command_info;
        result = vkQueueSubmit2(device->graphics_queue(), 1, &submit, fence);
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(
                detail::vulkan_error("could not submit the offscreen frame", "offscreen", result));
        }
        result = vkWaitForFences(device->device(), 1, &fence, VK_TRUE,
                                 std::numeric_limits<std::uint64_t>::max());
        if (result != VK_SUCCESS) {
            return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
                "could not wait for the offscreen frame", "offscreen", result));
        }
    }

    vmaInvalidateAllocation(device->allocator(), readback_allocation, 0, VK_WHOLE_SIZE);
    void* mapped = nullptr;
    const VkResult map_result = vmaMapMemory(device->allocator(), readback_allocation, &mapped);
    if (map_result != VK_SUCCESS) {
        return core::make_failure<std::vector<std::uint8_t>>(detail::vulkan_error(
            "could not map the offscreen readback buffer", "offscreen", map_result));
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4U);
    std::memcpy(pixels.data(), mapped, pixels.size());
    vmaUnmapMemory(device->allocator(), readback_allocation);
    return pixels;
}

OffscreenRenderer::OffscreenRenderer(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

OffscreenRenderer::OffscreenRenderer(OffscreenRenderer&&) noexcept = default;

OffscreenRenderer& OffscreenRenderer::operator=(OffscreenRenderer&&) noexcept = default;

OffscreenRenderer::~OffscreenRenderer() = default;

core::Result<OffscreenRenderer> OffscreenRenderer::create(const Device& device,
                                                          const OffscreenOptions& options) {
    auto impl = std::make_unique<Impl>();
    auto initialized = impl->initialize(device, options);
    if (!initialized) {
        const core::Error& error = initialized.error();
        return core::make_failure<OffscreenRenderer>(error);
    }
    return OffscreenRenderer(std::move(impl));
}

core::Result<std::vector<std::uint8_t>> OffscreenRenderer::render(const scene::SceneView& view,
                                                                  int frames) {
    return impl_->draw(view, frames);
}

std::uint32_t OffscreenRenderer::width() const noexcept {
    return impl_->width;
}

std::uint32_t OffscreenRenderer::height() const noexcept {
    return impl_->height;
}

} // namespace engine::render
