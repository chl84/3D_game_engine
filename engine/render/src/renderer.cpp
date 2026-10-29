#include <engine/render/renderer.hpp>

#include <engine/render/camera.hpp>
#include <engine/render/frustum.hpp>
#include <engine/render/scene_transform.hpp>
#include <engine/scene/scene_view.hpp>

#include "shaders.hpp"
#include "vulkan_util.hpp"

#include <SDL3/SDL_events.h>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine::render {
namespace {

constexpr std::size_t kFramesInFlight = 2;

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

static_assert(sizeof(PushConstants) == 128);

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
        0, 1, 2, 0, 2, 3, // front
        4, 6, 5, 4, 7, 6, // back
        0, 3, 7, 0, 7, 4, // left
        1, 5, 6, 1, 6, 2, // right
        3, 2, 6, 3, 6, 7, // top
        0, 4, 5, 0, 5, 1, // bottom
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

[[nodiscard]] VkClearValue clear_color() noexcept {
    VkClearValue value{};
    value.color = {{0.04F, 0.05F, 0.07F, 1.0F}};
    return value;
}

[[nodiscard]] bool is_srgb_format(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] VkSurfaceFormatKHR
choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    if (preferred != formats.end()) {
        return *preferred;
    }

    const auto srgb = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
        return is_srgb_format(format.format) &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    if (srgb != formats.end()) {
        return *srgb;
    }

    const auto nonlinear = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
        return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    if (nonlinear != formats.end()) {
        return *nonlinear;
    }

    return formats.front();
}

[[nodiscard]] VkCompositeAlphaFlagBitsKHR
choose_composite_alpha(const VkSurfaceCapabilitiesKHR& capabilities) noexcept {
    constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> kCandidates{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (const VkCompositeAlphaFlagBitsKHR candidate : kCandidates) {
        if ((capabilities.supportedCompositeAlpha &
             static_cast<VkCompositeAlphaFlagsKHR>(candidate)) != 0) {
            return candidate;
        }
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

[[nodiscard]] VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                                       const platform::Window& window) noexcept {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    const int width = window.width();
    const int height = window.height();
    if (width <= 0 || height <= 0) {
        return VkExtent2D{0, 0};
    }

    const auto clamp_extent = [](int value, std::uint32_t minimum, std::uint32_t maximum) {
        const auto extent = static_cast<std::uint32_t>(value);
        return std::min(std::max(extent, minimum), maximum);
    };
    return VkExtent2D{
        clamp_extent(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        clamp_extent(height, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height),
    };
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

struct Renderer::Impl {
    struct Frame {
        VkCommandPool command_pool{VK_NULL_HANDLE};
        VkCommandBuffer command_buffer{VK_NULL_HANDLE};
        VkSemaphore image_available{VK_NULL_HANDLE};
        VkFence in_flight{VK_NULL_HANDLE};
    };

    struct GpuMesh {
        VkBuffer vertex_buffer{VK_NULL_HANDLE};
        VmaAllocation vertex_allocation{nullptr};
        VkBuffer index_buffer{VK_NULL_HANDLE};
        VmaAllocation index_allocation{nullptr};
        std::uint32_t index_count{0};
        float radius{0.0F};
    };

    const Device* device{nullptr};
    const platform::Window* window{nullptr};
    SwapchainOptions options;
    bool overlay_visible{true};
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkSemaphore> render_finished;
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    GpuMesh cube_mesh;
    GpuMesh plane_mesh;
    VkImage depth_image{VK_NULL_HANDLE};
    VmaAllocation depth_allocation{nullptr};
    VkImageView depth_view{VK_NULL_HANDLE};
    VkFormat depth_format{VK_FORMAT_D32_SFLOAT};
    std::array<Frame, kFramesInFlight> frames{};
    std::size_t frame_index{0};
    std::uint64_t presented_frames{0};
    std::uint32_t last_visible_entities{0};
    int window_width{0};
    int window_height{0};
    FrameStats stats;
    std::chrono::steady_clock::time_point last_present;
    VkQueryPool timestamp_pool{VK_NULL_HANDLE};
    double timestamp_period{0.0};
    std::uint32_t timestamp_valid_bits{0};
    bool timestamp_supported{false};
    std::array<bool, kFramesInFlight> timestamp_ready{};
    bool overlay_context_initialized{false};
    bool overlay_platform_initialized{false};
    bool overlay_vulkan_initialized{false};
    std::uint64_t overlay_entity_count{0};
    std::uint32_t overlay_draw_vertices{0};

    ~Impl();

    [[nodiscard]] core::Result<void> initialize(const Device& render_device,
                                                const platform::Window& render_window,
                                                const SwapchainOptions& swapchain_options);
    [[nodiscard]] core::Result<void> create_frame_resources();
    void destroy_frame_resources();
    [[nodiscard]] core::Result<void> create_pipeline_layout();
    void destroy_pipeline_layout();
    [[nodiscard]] core::Result<void> create_builtin_meshes();
    void destroy_builtin_meshes();
    [[nodiscard]] core::Result<GpuMesh> create_gpu_mesh(const MeshData& mesh);
    void destroy_gpu_mesh(GpuMesh& mesh);
    [[nodiscard]] const GpuMesh* find_mesh(std::string_view mesh_id) const noexcept;
    [[nodiscard]] core::Result<void> create_depth_resources();
    void destroy_depth_resources();
    [[nodiscard]] core::Result<void> create_pipeline();
    [[nodiscard]] core::Result<void> create_timestamp_pool();
    void destroy_timestamp_pool();
    [[nodiscard]] core::Result<void> create_overlay_context();
    void destroy_overlay_context();
    [[nodiscard]] core::Result<void> create_overlay_vulkan();
    void destroy_overlay_vulkan();
    void build_overlay(const scene::SceneView& scene_view);
    [[nodiscard]] core::Result<void> create_swapchain();
    [[nodiscard]] core::Result<void> recreate_swapchain();
    void destroy_swapchain();
    [[nodiscard]] core::Result<void> record_frame(std::uint32_t image_index,
                                                  const scene::SceneView& scene_view,
                                                  const CameraPose* camera_pose);
};

Renderer::Impl::~Impl() {
    if (device != nullptr && device->device() != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device->device());
    }
    destroy_swapchain();
    destroy_overlay_context();
    destroy_timestamp_pool();
    destroy_builtin_meshes();
    destroy_pipeline_layout();
    destroy_frame_resources();
}

core::Result<void> Renderer::Impl::initialize(const Device& render_device,
                                              const platform::Window& render_window,
                                              const SwapchainOptions& swapchain_options) {
    if (!render_device.has_surface()) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Precondition,
                                                         "the device has no Vulkan surface", {},
                                                         {{"stage", "swapchain"}}));
    }
    const QueueFamilies& families = render_device.queue_families();
    if (!families.presentation || families.graphics != families.present) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Unsupported,
                             "the renderer requires one queue family with graphics and present", {},
                             {
                                 {"stage", "swapchain"},
                                 {"graphicsFamily", families.graphics},
                                 {"presentFamily", families.present},
                             }));
    }

    device = &render_device;
    window = &render_window;
    options = swapchain_options;
    overlay_visible = options.debug_overlay;

    auto overlay_context = create_overlay_context();
    if (!overlay_context) {
        return overlay_context;
    }
    auto timestamp_pool_result = create_timestamp_pool();
    if (!timestamp_pool_result) {
        return timestamp_pool_result;
    }
    auto frame_resources = create_frame_resources();
    if (!frame_resources) {
        return frame_resources;
    }
    auto pipeline_layout_result = create_pipeline_layout();
    if (!pipeline_layout_result) {
        return pipeline_layout_result;
    }
    auto meshes_result = create_builtin_meshes();
    if (!meshes_result) {
        return meshes_result;
    }
    return create_swapchain();
}

core::Result<void> Renderer::Impl::create_frame_resources() {
    const VkDevice handle = device->device();
    const std::uint32_t queue_family = device->queue_families().graphics;

    for (Frame& frame : frames) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family;
        VkResult result = vkCreateCommandPool(handle, &pool_info, nullptr, &frame.command_pool);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(
                detail::vulkan_error("could not create a command pool", "command-pool", result));
        }

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = frame.command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        result = vkAllocateCommandBuffers(handle, &allocate_info, &frame.command_buffer);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(detail::vulkan_error(
                "could not allocate a command buffer", "command-buffer", result));
        }

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        result = vkCreateSemaphore(handle, &semaphore_info, nullptr, &frame.image_available);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(detail::vulkan_error(
                "could not create the image-available semaphore", "frame-semaphore", result));
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        result = vkCreateFence(handle, &fence_info, nullptr, &frame.in_flight);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(
                detail::vulkan_error("could not create a frame fence", "frame-fence", result));
        }
    }

    return {};
}

// clang-tidy cannot see the writes through the frame array.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Renderer::Impl::destroy_frame_resources() {
    if (device == nullptr) {
        return;
    }
    const VkDevice handle = device->device();
    for (Frame& frame : frames) {
        if (frame.in_flight != VK_NULL_HANDLE) {
            vkDestroyFence(handle, frame.in_flight, nullptr);
            frame.in_flight = VK_NULL_HANDLE;
        }
        if (frame.image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(handle, frame.image_available, nullptr);
            frame.image_available = VK_NULL_HANDLE;
        }
        if (frame.command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(handle, frame.command_pool, nullptr);
            frame.command_pool = VK_NULL_HANDLE;
            frame.command_buffer = VK_NULL_HANDLE;
        }
    }
}

core::Result<void> Renderer::Impl::create_pipeline_layout() {
    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.pushConstantRangeCount = 1;
    create_info.pPushConstantRanges = &push_constant;

    const VkResult result =
        vkCreatePipelineLayout(device->device(), &create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the pipeline layout", "pipeline", result));
    }
    return {};
}

void Renderer::Impl::destroy_pipeline_layout() {
    if (device == nullptr || pipeline_layout == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipelineLayout(device->device(), pipeline_layout, nullptr);
    pipeline_layout = VK_NULL_HANDLE;
}

core::Result<Renderer::Impl::GpuMesh> Renderer::Impl::create_gpu_mesh(const MeshData& mesh) {
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
            return core::make_failure<void>(
                detail::vulkan_error("could not create a mesh buffer", "mesh", result));
        }
        return {};
    };

    GpuMesh gpu;
    auto vertex_buffer =
        create_buffer(mesh.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      gpu.vertex_buffer, gpu.vertex_allocation);
    if (!vertex_buffer) {
        return core::make_failure<GpuMesh>(vertex_buffer.error());
    }
    auto index_buffer =
        create_buffer(mesh.indices.size() * sizeof(std::uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      gpu.index_buffer, gpu.index_allocation);
    if (!index_buffer) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(index_buffer.error());
    }

    void* mapped = nullptr;
    VkResult result = vmaMapMemory(device->allocator(), gpu.vertex_allocation, &mapped);
    if (result != VK_SUCCESS) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(
            detail::vulkan_error("could not map a mesh vertex buffer", "mesh", result));
    }
    std::memcpy(mapped, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(device->allocator(), gpu.vertex_allocation);

    result = vmaMapMemory(device->allocator(), gpu.index_allocation, &mapped);
    if (result != VK_SUCCESS) {
        destroy_gpu_mesh(gpu);
        return core::make_failure<GpuMesh>(
            detail::vulkan_error("could not map a mesh index buffer", "mesh", result));
    }
    std::memcpy(mapped, mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    vmaUnmapMemory(device->allocator(), gpu.index_allocation);

    gpu.index_count = static_cast<std::uint32_t>(mesh.indices.size());
    gpu.radius = mesh.radius;
    return gpu;
}

// clang-tidy cannot see the resource writes through the reference.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Renderer::Impl::destroy_gpu_mesh(GpuMesh& mesh) {
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

core::Result<void> Renderer::Impl::create_builtin_meshes() {
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

void Renderer::Impl::destroy_builtin_meshes() {
    destroy_gpu_mesh(cube_mesh);
    destroy_gpu_mesh(plane_mesh);
}

const Renderer::Impl::GpuMesh* Renderer::Impl::find_mesh(std::string_view mesh_id) const noexcept {
    if (mesh_id == scene::kBuiltinCubeMesh) {
        return &cube_mesh;
    }
    if (mesh_id == scene::kBuiltinPlaneMesh) {
        return &plane_mesh;
    }
    return nullptr;
}

core::Result<void> Renderer::Impl::create_depth_resources() {
    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(device->physical_device(), depth_format,
                                        &format_properties);
    if ((format_properties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Unsupported,
                                                         "D32 depth attachment is not supported",
                                                         {}, {{"stage", "depth"}}));
    }

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = depth_format;
    image_info.extent = VkExtent3D{extent.width, extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(device->allocator(), &image_info, &allocation_info,
                                     &depth_image, &depth_allocation, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the depth image", "depth", result));
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = depth_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = depth_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device->device(), &view_info, nullptr, &depth_view);
    if (result != VK_SUCCESS) {
        destroy_depth_resources();
        return core::make_failure<void>(
            detail::vulkan_error("could not create the depth image view", "depth", result));
    }
    return {};
}

void Renderer::Impl::destroy_depth_resources() {
    if (device == nullptr) {
        return;
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

core::Result<void> Renderer::Impl::create_pipeline() {
    if (format == VK_FORMAT_UNDEFINED) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Precondition,
                                                         "the swapchain format is not available",
                                                         {}, {{"stage", "pipeline"}}));
    }
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->device(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

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

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = *vertex_module;
    shader_stages[0].pName = "main";
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = *fragment_module;
    shader_stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = static_cast<std::uint32_t>(sizeof(glm::vec3));

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

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
    depth_stencil.maxDepthBounds = 1.0F;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_G_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_B_BIT) |
                                      static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_A_BIT);
    blend_attachment.blendEnable = VK_FALSE;

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
    rendering.pColorAttachmentFormats = &format;
    rendering.depthAttachmentFormat = depth_format;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering;
    pipeline_info.stageCount = static_cast<std::uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.subpass = 0;

    VkPipeline new_pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(device->device(), VK_NULL_HANDLE, 1,
                                                      &pipeline_info, nullptr, &new_pipeline);
    vkDestroyShaderModule(device->device(), *vertex_module, nullptr);
    vkDestroyShaderModule(device->device(), *fragment_module, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the graphics pipeline", "pipeline", result));
    }

    pipeline = new_pipeline;
    return {};
}

core::Result<void> Renderer::Impl::create_timestamp_pool() {
    timestamp_supported = false;
    stats.gpu_timing_supported = false;

    const std::uint32_t graphics_family = device->queue_families().graphics;
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device(), &family_count, nullptr);
    if (family_count <= graphics_family) {
        return {};
    }
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device(), &family_count,
                                             families.data());

    const float period = device->properties().limits.timestampPeriod;
    if (families[graphics_family].timestampValidBits == 0 || period <= 0.0F) {
        return {};
    }

    VkQueryPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    create_info.queryCount = static_cast<std::uint32_t>(kFramesInFlight * 2);
    VkQueryPool pool = VK_NULL_HANDLE;
    const VkResult result = vkCreateQueryPool(device->device(), &create_info, nullptr, &pool);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the timestamp query pool", "timestamp", result));
    }

    timestamp_pool = pool;
    timestamp_period = static_cast<double>(period);
    timestamp_valid_bits = families[graphics_family].timestampValidBits;
    timestamp_supported = true;
    stats.gpu_timing_supported = true;
    return {};
}

void Renderer::Impl::destroy_timestamp_pool() {
    if (device != nullptr && timestamp_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device->device(), timestamp_pool, nullptr);
        timestamp_pool = VK_NULL_HANDLE;
    }
    timestamp_supported = false;
}

core::Result<void> Renderer::Impl::create_overlay_context() {
    if (!options.debug_overlay) {
        return {};
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Precondition,
                             "an ImGui context already exists; the renderer owns its own context",
                             {}, {{"stage", "overlay"}}));
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // ImGui stores configuration flags as a signed int.
    // NOLINTNEXTLINE(bugprone-signed-bitwise)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    overlay_context_initialized = true;

    if (!ImGui_ImplSDL3_InitForVulkan(window->native_handle())) {
        destroy_overlay_context();
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Internal, "could not initialize the ImGui SDL3 backend", {},
            {{"stage", "overlay"}}));
    }
    overlay_platform_initialized = true;
    return {};
}

void Renderer::Impl::destroy_overlay_context() {
    if (!overlay_context_initialized) {
        return;
    }
    if (overlay_platform_initialized) {
        ImGui_ImplSDL3_Shutdown();
        overlay_platform_initialized = false;
    }
    ImGui::DestroyContext();
    overlay_context_initialized = false;
}

core::Result<void> Renderer::Impl::create_overlay_vulkan() {
    if (!options.debug_overlay || !overlay_context_initialized || image_views.empty()) {
        return {};
    }

    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = device->instance();
    info.PhysicalDevice = device->physical_device();
    info.Device = device->device();
    info.QueueFamily = device->queue_families().graphics;
    info.Queue = device->graphics_queue();
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    info.MinImageCount = 2;
    info.ImageCount = static_cast<std::uint32_t>(std::max<std::size_t>(image_views.size(), 2));
    info.MinAllocationSize = static_cast<VkDeviceSize>(1024 * 1024);
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &format;
    // ImGui does not depth-test, but the pipeline must declare the depth
    // attachment that the current dynamic-rendering pass uses.
    rendering.depthAttachmentFormat = depth_format;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = rendering;

    if (!ImGui_ImplVulkan_Init(&info)) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Internal, "could not initialize the ImGui Vulkan backend", {},
            {{"stage", "overlay"}}));
    }
    overlay_vulkan_initialized = true;
    return {};
}

void Renderer::Impl::destroy_overlay_vulkan() {
    if (overlay_vulkan_initialized) {
        ImGui_ImplVulkan_Shutdown();
        overlay_vulkan_initialized = false;
    }
}

void Renderer::Impl::build_overlay(const scene::SceneView& scene_view) {
    if (!overlay_vulkan_initialized || !overlay_visible) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(16.0F, 16.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0F, 420.0F), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Engine debug")) {
        ImGui::Text("FPS %.1f", stats.frames_per_second);
        ImGui::Text("CPU %.2f ms", stats.cpu_frame_ms);
        if (stats.gpu_timing_supported) {
            ImGui::Text("GPU %.2f ms", stats.gpu_frame_ms);
        } else {
            ImGui::TextUnformatted("GPU n/a");
        }
        ImGui::Separator();

        const std::vector<scene::Entity> entities = scene_view.entities();
        ImGui::Text("%zu entities", entities.size());
        if (ImGui::BeginChild("entity-list", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
            for (const scene::Entity entity : entities) {
                const std::string_view name = scene_view.name(entity);
                ImGui::Text("%.*s", static_cast<int>(name.size()), name.data());
                if (scene_view.has_camera(entity)) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[camera]");
                }
                if (scene_view.has_mesh(entity)) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", scene_view.mesh(entity).mesh_id.c_str());
                }
            }
        }
        ImGui::EndChild();
        overlay_entity_count = static_cast<std::uint64_t>(entities.size());
    }
    ImGui::End();
    ImGui::Render();
}

core::Result<void> Renderer::Impl::create_swapchain() {
    if (window->minimized()) {
        return {};
    }

    const VkPhysicalDevice physical_device = device->physical_device();
    const VkSurfaceKHR surface = device->surface();

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not query the surface capabilities", "swapchain", result));
    }

    std::uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    if (result != VK_SUCCESS || format_count == 0) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Unsupported,
                                                         "the surface has no swapchain formats", {},
                                                         {{"stage", "swapchain"}}));
    }
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                                  formats.data());
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not query the surface formats", "swapchain", result));
    }

    std::uint32_t mode_count = 0;
    result =
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, nullptr);
    if (result != VK_SUCCESS || mode_count == 0) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Unsupported,
                                                         "the surface has no present modes", {},
                                                         {{"stage", "swapchain"}}));
    }
    std::vector<VkPresentModeKHR> present_modes(mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count,
                                                       present_modes.data());
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not query the present modes", "swapchain", result));
    }
    if (options.present_mode != VK_PRESENT_MODE_FIFO_KHR &&
        std::ranges::find(present_modes, options.present_mode) == present_modes.end()) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Unsupported, "the requested present mode is not supported", {},
            {{"stage", "swapchain"}, {"presentMode", static_cast<int>(options.present_mode)}}));
    }

    if ((capabilities.supportedUsageFlags &
         static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) == 0) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Unsupported, "the swapchain cannot be used as a color attachment", {},
            {{"stage", "swapchain"}}));
    }

    const VkExtent2D swapchain_extent = choose_extent(capabilities, *window);
    if (swapchain_extent.width == 0 || swapchain_extent.height == 0) {
        return {};
    }

    const VkSurfaceFormatKHR surface_format = choose_surface_format(formats);
    std::uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    // Vulkan requires every unspecified field to be zero.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = choose_composite_alpha(capabilities);
    create_info.presentMode = options.present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    result = vkCreateSwapchainKHR(device->device(), &create_info, nullptr, &new_swapchain);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not create the swapchain", "swapchain", result));
    }

    std::uint32_t actual_image_count = 0;
    result = vkGetSwapchainImagesKHR(device->device(), new_swapchain, &actual_image_count, nullptr);
    if (result != VK_SUCCESS || actual_image_count == 0) {
        vkDestroySwapchainKHR(device->device(), new_swapchain, nullptr);
        return core::make_failure<void>(
            detail::vulkan_error("could not query the swapchain images", "swapchain", result));
    }

    std::vector<VkImage> new_images(actual_image_count);
    result = vkGetSwapchainImagesKHR(device->device(), new_swapchain, &actual_image_count,
                                     new_images.data());
    if (result != VK_SUCCESS) {
        vkDestroySwapchainKHR(device->device(), new_swapchain, nullptr);
        return core::make_failure<void>(
            detail::vulkan_error("could not query the swapchain images", "swapchain", result));
    }

    std::vector<VkImageView> new_views;
    new_views.reserve(new_images.size());
    for (const VkImage image : new_images) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = surface_format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        result = vkCreateImageView(device->device(), &view_info, nullptr, &view);
        if (result != VK_SUCCESS) {
            for (const VkImageView created : new_views) {
                vkDestroyImageView(device->device(), created, nullptr);
            }
            vkDestroySwapchainKHR(device->device(), new_swapchain, nullptr);
            return core::make_failure<void>(detail::vulkan_error(
                "could not create a swapchain image view", "swapchain", result));
        }
        new_views.push_back(view);
    }

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    std::vector<VkSemaphore> new_render_finished;
    new_render_finished.reserve(new_images.size());
    for (std::size_t index = 0; index < new_images.size(); ++index) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        result = vkCreateSemaphore(device->device(), &semaphore_info, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            for (const VkSemaphore created : new_render_finished) {
                vkDestroySemaphore(device->device(), created, nullptr);
            }
            for (const VkImageView created : new_views) {
                vkDestroyImageView(device->device(), created, nullptr);
            }
            vkDestroySwapchainKHR(device->device(), new_swapchain, nullptr);
            return core::make_failure<void>(detail::vulkan_error(
                "could not create a render-finished semaphore", "swapchain", result));
        }
        new_render_finished.push_back(semaphore);
    }

    swapchain = new_swapchain;
    format = surface_format.format;
    extent = swapchain_extent;
    images = std::move(new_images);
    image_views = std::move(new_views);
    render_finished = std::move(new_render_finished);
    window_width = window->width();
    window_height = window->height();

    auto depth_result = create_depth_resources();
    if (!depth_result) {
        destroy_swapchain();
        return depth_result;
    }

    auto pipeline_result = create_pipeline();
    if (!pipeline_result) {
        destroy_swapchain();
        return pipeline_result;
    }

    auto overlay_result = create_overlay_vulkan();
    if (!overlay_result) {
        destroy_swapchain();
        return overlay_result;
    }
    return {};
}

core::Result<void> Renderer::Impl::recreate_swapchain() {
    const VkResult result = vkDeviceWaitIdle(device->device());
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not wait for the device", "swapchain", result));
    }
    destroy_swapchain();
    return create_swapchain();
}

void Renderer::Impl::destroy_swapchain() {
    if (device == nullptr) {
        return;
    }
    destroy_overlay_vulkan();
    const VkDevice handle = device->device();
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(handle, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    destroy_depth_resources();
    for (const VkSemaphore semaphore : render_finished) {
        vkDestroySemaphore(handle, semaphore, nullptr);
    }
    render_finished.clear();
    for (const VkImageView view : image_views) {
        vkDestroyImageView(handle, view, nullptr);
    }
    image_views.clear();
    images.clear();
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(handle, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    format = VK_FORMAT_UNDEFINED;
    extent = VkExtent2D{0, 0};
}

core::Result<void> Renderer::Impl::record_frame(std::uint32_t image_index,
                                                const scene::SceneView& scene_view,
                                                const CameraPose* camera_pose) {
    const auto camera_entity = scene_view.active_camera();
    if (!camera_entity.has_value()) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::NotFound, "the scene has no active camera", {}, {{"stage", "scene"}}));
    }
    scene::WorldTransform camera_world;
    if (camera_pose != nullptr) {
        camera_world.position = camera_pose->position;
        camera_world.rotation = camera_pose->rotation;
    } else {
        camera_world = scene_view.world_transform(*camera_entity);
    }
    const scene::Camera& camera_component = scene_view.camera(*camera_entity);

    Camera camera;
    camera.vertical_fov_radians = camera_component.fovYRadians;
    camera.near_plane = camera_component.near;
    camera.far_plane = camera_component.far;

    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const glm::mat4 view_projection =
        camera.projection(aspect) * camera_view_from_world(camera_world);
    const Frustum frustum = Frustum::from_view_projection(view_projection);

    Frame& frame = frames[frame_index];
    VkResult result = vkResetCommandBuffer(frame.command_buffer, 0);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not reset the command buffer", "command-buffer", result));
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(frame.command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not begin the command buffer", "command-buffer", result));
    }

    const auto first_query = static_cast<std::uint32_t>(frame_index * 2);
    if (timestamp_supported) {
        vkCmdResetQueryPool(frame.command_buffer, timestamp_pool, first_query, 2);
        vkCmdWriteTimestamp2(frame.command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             timestamp_pool, first_query);
    }

    transition_image(frame.command_buffer, images[image_index], VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    constexpr VkPipelineStageFlags2 kDepthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    transition_image(frame.command_buffer, depth_image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, kDepthStages, 0, kDepthStages,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = image_views[image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue = clear_color();

    VkRenderingAttachmentInfo depth_attachment{};
    depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView = depth_view;
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.clearValue.depthStencil.depth = 1.0F;
    depth_attachment.clearValue.depthStencil.stencil = 0;

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = VkOffset2D{0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    vkCmdBeginRendering(frame.command_buffer, &rendering_info);

    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{};
    scissor.offset = VkOffset2D{0, 0};
    scissor.extent = extent;

    vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(frame.command_buffer, 0, 1, &scissor);
    vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    PushConstants push_constants;
    push_constants.view_projection = view_projection;
    std::uint32_t visible = 0;
    for (const scene::Entity entity : scene_view.entities()) {
        if (!scene_view.has_mesh(entity)) {
            continue;
        }
        const scene::Mesh& mesh_component = scene_view.mesh(entity);
        const GpuMesh* mesh = find_mesh(mesh_component.mesh_id);
        if (mesh == nullptr) {
            return core::make_failure<void>(core::make_error(
                core::ErrorCode::Unsupported, "the scene references an unknown mesh", {},
                {{"stage", "mesh"}, {"meshId", mesh_component.mesh_id}}));
        }

        const scene::WorldTransform world = scene_view.world_transform(entity);
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
        vkCmdPushConstants(frame.command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstants), &push_constants);
        VkDeviceSize vertex_offset = 0;
        vkCmdBindVertexBuffers(frame.command_buffer, 0, 1, &mesh->vertex_buffer, &vertex_offset);
        vkCmdBindIndexBuffer(frame.command_buffer, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(frame.command_buffer, mesh->index_count, 1, 0, 0, 0);
        ++visible;
    }
    last_visible_entities = visible;

    if (overlay_vulkan_initialized && overlay_visible) {
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (draw_data != nullptr) {
            overlay_draw_vertices = static_cast<std::uint32_t>(draw_data->TotalVtxCount);
            ImGui_ImplVulkan_RenderDrawData(draw_data, frame.command_buffer);
        }
    } else {
        overlay_draw_vertices = 0;
    }

    vkCmdEndRendering(frame.command_buffer);

    transition_image(frame.command_buffer, images[image_index],
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0,
                     VK_IMAGE_ASPECT_COLOR_BIT);

    if (timestamp_supported) {
        vkCmdWriteTimestamp2(frame.command_buffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             timestamp_pool, first_query + 1);
    }

    result = vkEndCommandBuffer(frame.command_buffer);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not end the command buffer", "command-buffer", result));
    }
    return {};
}

core::Result<Renderer> Renderer::create(const Device& device, const platform::Window& window,
                                        const SwapchainOptions& options) {
    auto impl = std::make_unique<Impl>();
    auto initialized = impl->initialize(device, window, options);
    if (!initialized) {
        const core::Error& error = initialized.error();
        return core::make_failure<Renderer>(error);
    }
    return Renderer(std::move(impl));
}

Renderer::Renderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Renderer::Renderer(Renderer&&) noexcept = default;

Renderer& Renderer::operator=(Renderer&&) noexcept = default;

Renderer::~Renderer() = default;

core::Result<bool> Renderer::draw_frame(const scene::SceneView& view) {
    return draw_frame_impl(view, nullptr);
}

core::Result<bool> Renderer::draw_frame(const scene::SceneView& view, const CameraPose& camera) {
    return draw_frame_impl(view, &camera);
}

core::Result<bool> Renderer::draw_frame_impl(const scene::SceneView& view,
                                             const CameraPose* camera) {
    if (impl_->window->minimized()) {
        return false;
    }
    if (impl_->swapchain == VK_NULL_HANDLE) {
        auto created = impl_->create_swapchain();
        if (!created) {
            return core::make_failure<bool>(created.error());
        }
        if (impl_->swapchain == VK_NULL_HANDLE) {
            return false;
        }
    }

    const int pixel_width = impl_->window->width();
    const int pixel_height = impl_->window->height();
    if (pixel_width <= 0 || pixel_height <= 0) {
        return false;
    }
    if (pixel_width != impl_->window_width || pixel_height != impl_->window_height) {
        auto recreated = impl_->recreate_swapchain();
        if (!recreated) {
            return core::make_failure<bool>(recreated.error());
        }
        if (impl_->swapchain == VK_NULL_HANDLE) {
            return false;
        }
    }

    if (!view.active_camera().has_value()) {
        return core::make_failure<bool>(core::make_error(
            core::ErrorCode::NotFound, "the scene has no active camera", {}, {{"stage", "scene"}}));
    }
    for (const scene::Entity entity : view.entities()) {
        if (!view.has_mesh(entity)) {
            continue;
        }
        if (impl_->find_mesh(view.mesh(entity).mesh_id) == nullptr) {
            return core::make_failure<bool>(core::make_error(
                core::ErrorCode::Unsupported, "the scene references an unknown mesh", {},
                {{"stage", "mesh"}, {"meshId", view.mesh(entity).mesh_id}}));
        }
    }

    Impl::Frame& frame = impl_->frames[impl_->frame_index];
    VkResult result = vkWaitForFences(impl_->device->device(), 1, &frame.in_flight, VK_TRUE,
                                      std::numeric_limits<std::uint64_t>::max());
    if (result != VK_SUCCESS) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not wait for the frame fence", "frame", result));
    }

    const std::size_t frame_slot = impl_->frame_index;
    if (impl_->timestamp_supported && impl_->timestamp_ready[frame_slot]) {
        std::array<std::uint64_t, 2> timestamps{};
        const VkResult query =
            vkGetQueryPoolResults(impl_->device->device(), impl_->timestamp_pool,
                                  static_cast<std::uint32_t>(frame_slot * 2), 2, sizeof(timestamps),
                                  timestamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
        const std::uint64_t mask =
            impl_->timestamp_valid_bits >= 64
                ? ~std::uint64_t{0}
                : ((std::uint64_t{1} << impl_->timestamp_valid_bits) - std::uint64_t{1});
        const std::uint64_t start = timestamps[0] & mask;
        const std::uint64_t end = timestamps[1] & mask;
        if (query == VK_SUCCESS && end >= start) {
            impl_->stats.gpu_frame_ms =
                static_cast<double>(end - start) * impl_->timestamp_period / 1'000'000.0;
            ++impl_->stats.gpu_samples;
        }
    }
    const auto cpu_start = std::chrono::steady_clock::now();

    std::uint32_t image_index = 0;
    result = vkAcquireNextImageKHR(impl_->device->device(), impl_->swapchain,
                                   std::numeric_limits<std::uint64_t>::max(), frame.image_available,
                                   VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        auto recreated = impl_->recreate_swapchain();
        if (!recreated) {
            return core::make_failure<bool>(recreated.error());
        }
        return false;
    }
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::make_failure<bool>(
            detail::vulkan_error("the surface was lost", "swapchain", result));
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not acquire a swapchain image", "swapchain", result));
    }
    const bool suboptimal = result == VK_SUBOPTIMAL_KHR;

    result = vkResetFences(impl_->device->device(), 1, &frame.in_flight);
    if (result != VK_SUCCESS) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not reset the frame fence", "frame", result));
    }

    impl_->build_overlay(view);
    auto recorded = impl_->record_frame(image_index, view, camera);
    if (!recorded) {
        return core::make_failure<bool>(recorded.error());
    }

    VkSemaphoreSubmitInfo wait_info{};
    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_info.semaphore = frame.image_available;
    wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signal_info{};
    signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_info.semaphore = impl_->render_finished[image_index];
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = frame.command_buffer;

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount = 1;
    submit_info.pWaitSemaphoreInfos = &wait_info;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_info;

    result = vkQueueSubmit2(impl_->device->graphics_queue(), 1, &submit_info, frame.in_flight);
    if (result != VK_SUCCESS) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not submit the frame", "frame", result));
    }
    impl_->timestamp_ready[frame_slot] = impl_->timestamp_supported;
    const auto cpu_end = std::chrono::steady_clock::now();
    impl_->stats.cpu_frame_ms =
        std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &impl_->render_finished[image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &impl_->swapchain;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(impl_->device->present_queue(), &present_info);
    impl_->frame_index = (impl_->frame_index + 1) % kFramesInFlight;

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        auto recreated = impl_->recreate_swapchain();
        if (!recreated) {
            return core::make_failure<bool>(recreated.error());
        }
        return false;
    }
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::make_failure<bool>(
            detail::vulkan_error("the surface was lost while presenting", "swapchain", result));
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not present the frame", "swapchain", result));
    }

    ++impl_->presented_frames;
    impl_->stats.frames = impl_->presented_frames;
    const auto presented_at = std::chrono::steady_clock::now();
    if (impl_->last_present.time_since_epoch().count() != 0) {
        const double seconds =
            std::chrono::duration<double>(presented_at - impl_->last_present).count();
        if (seconds > 0.0) {
            const double instant = 1.0 / seconds;
            impl_->stats.frames_per_second =
                impl_->stats.frames_per_second > 0.0
                    ? (impl_->stats.frames_per_second * 0.9) + (instant * 0.1)
                    : instant;
        }
    }
    impl_->last_present = presented_at;

    if (suboptimal || result == VK_SUBOPTIMAL_KHR) {
        auto recreated = impl_->recreate_swapchain();
        if (!recreated) {
            return core::make_failure<bool>(recreated.error());
        }
    }
    return true;
}

core::Result<void> Renderer::wait_idle() const {
    const VkResult result = vkDeviceWaitIdle(impl_->device->device());
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            detail::vulkan_error("could not wait for the device", "device", result));
    }
    return {};
}

bool Renderer::has_swapchain() const noexcept {
    return impl_->swapchain != VK_NULL_HANDLE;
}

VkSwapchainKHR Renderer::swapchain() const noexcept {
    return impl_->swapchain;
}

VkExtent2D Renderer::extent() const noexcept {
    return impl_->extent;
}

VkFormat Renderer::color_format() const noexcept {
    return impl_->format;
}

std::uint32_t Renderer::image_count() const noexcept {
    return static_cast<std::uint32_t>(impl_->images.size());
}

std::uint64_t Renderer::presented_frames() const noexcept {
    return impl_->presented_frames;
}

std::uint32_t Renderer::last_visible_entities() const noexcept {
    return impl_->last_visible_entities;
}

FrameStats Renderer::frame_stats() const noexcept {
    return impl_->stats;
}

bool Renderer::overlay_enabled() const noexcept {
    return impl_->options.debug_overlay;
}

bool Renderer::overlay_visible() const noexcept {
    return impl_->overlay_visible && impl_->overlay_vulkan_initialized;
}

void Renderer::set_overlay_visible(bool visible) noexcept {
    impl_->overlay_visible = impl_->options.debug_overlay && visible;
}

void Renderer::process_sdl_event(const SDL_Event& event) noexcept {
    if (impl_->overlay_context_initialized && impl_->overlay_visible) {
        (void)ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

bool Renderer::overlay_wants_mouse() const noexcept {
    return overlay_visible() && ImGui::GetCurrentContext() != nullptr &&
           ImGui::GetIO().WantCaptureMouse;
}

bool Renderer::overlay_wants_keyboard() const noexcept {
    return overlay_visible() && ImGui::GetCurrentContext() != nullptr &&
           ImGui::GetIO().WantCaptureKeyboard;
}

std::uint64_t Renderer::overlay_entity_count() const noexcept {
    return impl_->overlay_entity_count;
}

std::uint32_t Renderer::overlay_draw_vertices() const noexcept {
    return impl_->overlay_draw_vertices;
}

} // namespace engine::render
