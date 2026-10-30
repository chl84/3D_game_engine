#include "pbr_shaders.hpp"
#include "vulkan_util.hpp"
#include <engine/core/sha256.hpp>
#include <engine/render/camera.hpp>
#include <engine/render/pbr_renderer.hpp>
#include <engine/render/scene_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <utility>

namespace engine::render {
namespace {
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

using core::Result;
constexpr auto none = std::numeric_limits<std::uint32_t>::max();
template <class T> T info(VkStructureType type) {
    // Vulkan requires zeroed unspecified fields; callers assign required enums.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    T value{};
    value.sType = type;
    return value;
}
Result<void> checked(VkResult result, const char* message) {
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(detail::vulkan_error(message, "pbr", result));
    }
    return {};
}
template <class T> Result<T> invalid(const char* message) {
    return core::make_failure<T>(
        core::make_error(core::ErrorCode::Validation, message, {}, {{"stage", "pbr"}}));
}
struct Buffer {
    const Device* device{};
    VkBuffer handle{};
    VmaAllocation allocation{};
    void* mapped{};
    ~Buffer() {
        if (handle) {
            vmaDestroyBuffer(device->allocator(), handle, allocation);
        }
    }
};
Result<std::unique_ptr<Buffer>> buffer(const Device& device, VkDeviceSize size,
                                       VkBufferUsageFlags usage) {
    auto result = std::make_unique<Buffer>();
    result->device = &device;
    auto create = info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    create.size = size;
    create.usage = usage;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO;
    allocation.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo allocated{};
    auto status = checked(vmaCreateBuffer(device.allocator(), &create, &allocation, &result->handle,
                                          &result->allocation, &allocated),
                          "PBR buffer allocation failed");
    if (!status) {
        return core::make_failure<std::unique_ptr<Buffer>>(status.error());
    }
    result->mapped = allocated.pMappedData;
    if (device.gpu_memory_bytes() > kTotalGpuMemoryBudgetBytes) {
        return invalid<std::unique_ptr<Buffer>>("GPU memory budget exceeded");
    }
    return result;
}
Result<void> write(Buffer& target, const void* data, std::size_t size) {
    std::memcpy(target.mapped, data, size);
    return checked(vmaFlushAllocation(target.device->allocator(), target.allocation, 0, size),
                   "PBR memory flush failed");
}
struct Texture {
    const Device* device{};
    VkImage image{};
    VkImageView view{};
    VmaAllocation allocation{};
    VkDeviceSize bytes{};
    bool charged{false};
    ~Texture() {
        if (view) {
            vkDestroyImageView(device->device(), view, nullptr);
        }
        if (image) {
            vmaDestroyImage(device->allocator(), image, allocation);
        }
        if (charged) {
            device->release_texture_bytes(bytes);
        }
    }
};
struct Slot {
    std::vector<std::uint32_t>* free{};
    std::uint32_t index{};
    ~Slot() {
        free->push_back(index);
    }
};
std::unique_ptr<Slot> take(std::vector<std::uint32_t>& free) {
    auto lease = std::make_unique<Slot>();
    lease->free = &free;
    lease->index = free.back();
    free.pop_back();
    return lease;
}
struct Material {
    glm::vec4 base{1};
    glm::vec4 factors{1, 1, 1, 1};
    glm::vec4 emissive{0};
    glm::uvec4 textures{none};
    glm::uvec4 flags{none, 0, 0, 0};
};
struct TextureInfo {
    glm::uvec4 sampler{9729, 9987, 10497, 10497};
};
struct Records {
    std::array<Material, 4096> materials;
    std::array<TextureInfo, 4096> textures;
};
struct Frame {
    glm::vec4 direction{0, -1, 0, 0};
    glm::vec4 radiance{0};
    glm::vec4 ambient{0.03F, 0.03F, 0.03F, 0};
};
struct Push {
    glm::mat4 vp{1};
    glm::vec4 row0{}, row1{}, row2{};
    std::uint32_t material{}, encode_srgb{};
    std::array<std::uint32_t, 2> padding{};
};
static_assert(sizeof(Material) == 80 && sizeof(TextureInfo) == 16 && sizeof(Push) == 128);
struct Mesh {
    std::unique_ptr<Buffer> vertices, indices;
    std::uint32_t count{}, material{};
};
struct NodeDraw {
    glm::dmat4 transform{1};
    std::size_t mesh{};
};
struct Asset {
    std::vector<std::unique_ptr<Slot>> texture_slots, material_slots;
    std::vector<std::shared_ptr<Texture>> textures;
    std::vector<std::vector<Mesh>> meshes;
    std::vector<NodeDraw> nodes;
};
struct Draw {
    const Mesh* mesh{};
    Push push;
};
void barrier(VkCommandBuffer cmd, VkImage image, std::uint32_t base, std::uint32_t count,
             VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src,
             VkAccessFlags2 src_access, VkPipelineStageFlags2 dst, VkAccessFlags2 dst_access) {
    auto b = info<VkImageMemoryBarrier2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
    b.srcStageMask = src;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst;
    b.dstAccessMask = dst_access;
    b.oldLayout = old_layout;
    b.newLayout = new_layout;
    b.image = image;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, base, count, 0, 1};
    auto d = info<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &d);
}
struct Upload {
    const Device* device{};
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
    ~Upload() {
        if (fence) {
            vkDestroyFence(device->device(), fence, nullptr);
        }
        if (pool) {
            vkDestroyCommandPool(device->device(), pool, nullptr);
        }
    }
    Result<void> start(const Device& d) {
        device = &d;
        auto p = info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        p.queueFamilyIndex = d.queue_families().graphics;
        auto r = checked(vkCreateCommandPool(d.device(), &p, nullptr, &pool),
                         "PBR upload command pool failed");
        if (!r) {
            return r;
        }
        auto a = info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        a.commandPool = pool;
        a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        a.commandBufferCount = 1;
        r = checked(vkAllocateCommandBuffers(d.device(), &a, &cmd),
                    "PBR upload command buffer failed");
        if (!r) {
            return r;
        }
        auto f = info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        r = checked(vkCreateFence(d.device(), &f, nullptr, &fence), "PBR upload fence failed");
        if (!r) {
            return r;
        }
        auto b = info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        return checked(vkBeginCommandBuffer(cmd, &b), "PBR upload begin failed");
    }
    Result<void> finish() {
        auto r = checked(vkEndCommandBuffer(cmd), "PBR upload end failed");
        if (!r) {
            return r;
        }
        auto c = info<VkCommandBufferSubmitInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO);
        c.commandBuffer = cmd;
        auto s = info<VkSubmitInfo2>(VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
        s.commandBufferInfoCount = 1;
        s.pCommandBufferInfos = &c;
        r = checked(vkQueueSubmit2(device->graphics_queue(), 1, &s, fence),
                    "PBR upload submit failed");
        if (!r) {
            return r;
        }
        r = checked(vkWaitForFences(device->device(), 1, &fence, VK_TRUE, UINT64_MAX),
                    "PBR upload wait failed");
        if (!r) {
            (void)device->wait_idle();
        }
        return r;
    }
};
Result<std::unique_ptr<Texture>> texture(const Device& device, const asset::CookedTexture& source) {
    if (!source.width || !source.height || source.width > 8192 || source.height > 8192 ||
        source.rgba8.size() != static_cast<std::size_t>(source.width) * source.height * 4U) {
        return invalid<std::unique_ptr<Texture>>("invalid cooked texture dimensions or byte count");
    }
    if (source.width > device.properties().limits.maxImageDimension2D ||
        source.height > device.properties().limits.maxImageDimension2D) {
        return invalid<std::unique_ptr<Texture>>("texture exceeds device image dimensions");
    }
    const auto& sampler = source.sampler;
    const auto valid_wrap = [](asset::Wrap value) {
        return value == asset::Wrap::Repeat || value == asset::Wrap::MirroredRepeat ||
               value == asset::Wrap::ClampToEdge;
    };
    const auto valid_filter = [](asset::Filter value) {
        return value == asset::Filter::Nearest || value == asset::Filter::Linear ||
               value == asset::Filter::NearestMipmapNearest ||
               value == asset::Filter::LinearMipmapNearest ||
               value == asset::Filter::NearestMipmapLinear ||
               value == asset::Filter::LinearMipmapLinear;
    };
    if (!valid_wrap(sampler.wrap_s) || !valid_wrap(sampler.wrap_t) ||
        !valid_filter(sampler.min_filter) ||
        (sampler.mag_filter != asset::Filter::Nearest &&
         sampler.mag_filter != asset::Filter::Linear)) {
        return invalid<std::unique_ptr<Texture>>("unsupported cooked sampler state");
    }
    auto t = std::make_unique<Texture>();
    t->device = &device;
    const VkFormat format = source.color_space == asset::TextureColorSpace::Srgb
                                ? VK_FORMAT_R8G8B8A8_SRGB
                                : VK_FORMAT_R8G8B8A8_UNORM;
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device.physical_device(), format, &properties);
    constexpr VkFormatFeatureFlags features =
        static_cast<VkFormatFeatureFlags>(VK_FORMAT_FEATURE_BLIT_SRC_BIT) |
        VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((properties.optimalTilingFeatures & features) != features) {
        return core::make_failure<std::unique_ptr<Texture>>(
            core::make_error(core::ErrorCode::Unsupported, "PBR mip blitting is not supported"));
    }
    const auto levels =
        static_cast<std::uint32_t>(std::floor(std::log2(std::max(source.width, source.height)))) +
        1U;
    auto image = info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = format;
    image.extent = {source.width, source.height, 1};
    image.mipLevels = levels;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationInfo allocated{};
    auto r = checked(
        vmaCreateImage(device.allocator(), &image, &alloc, &t->image, &t->allocation, &allocated),
        "PBR texture allocation failed");
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    t->bytes = allocated.size;
    r = device.charge_texture_bytes(t->bytes);
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    t->charged = true;
    auto view = info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    view.image = t->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
    r = checked(vkCreateImageView(device.device(), &view, nullptr, &t->view),
                "PBR texture view failed");
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    auto staging = buffer(device, source.rgba8.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!staging) {
        return core::make_failure<std::unique_ptr<Texture>>(staging.error());
    }
    r = write(**staging, source.rgba8.data(), source.rgba8.size());
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    Upload upload;
    r = upload.start(device);
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    barrier(upload.cmd, t->image, 0, levels, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = image.extent;
    vkCmdCopyBufferToImage(upload.cmd, (*staging)->handle, t->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    std::int32_t w = static_cast<std::int32_t>(source.width);
    std::int32_t h = static_cast<std::int32_t>(source.height);
    for (std::uint32_t level = 1; level < levels; ++level) {
        barrier(upload.cmd, t->image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT);
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
        blit.srcOffsets[1] = {w, h, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        blit.dstOffsets[1] = {std::max(w / 2, 1), std::max(h / 2, 1), 1};
        vkCmdBlitImage(upload.cmd, t->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        barrier(upload.cmd, t->image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }
    barrier(upload.cmd, t->image, levels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    r = upload.finish();
    if (!r) {
        return core::make_failure<std::unique_ptr<Texture>>(r.error());
    }
    return t;
}
} // namespace

struct PbrRenderer::Impl {
    const Device* device{};
    std::vector<std::uint32_t> free_textures, free_materials;
    std::map<std::string, std::unique_ptr<Asset>> assets;
    std::vector<std::unique_ptr<Asset>> retired;
    std::unique_ptr<Records> records{std::make_unique<Records>()};
    std::unique_ptr<Buffer> record_buffer;
    std::vector<std::unique_ptr<Buffer>> frames;
    std::vector<std::vector<Draw>> draws;
    VkDescriptorSetLayout frame_layout{}, bindless_layout{};
    VkDescriptorPool frame_pool{}, bindless_pool{};
    VkDescriptorSet bindless_set{};
    std::vector<VkDescriptorSet> frame_sets;
    std::array<VkSampler, 2> samplers{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    VkFormat color{}, depth{};
    std::uint64_t overhead{};
    ~Impl() {
        if (!device) {
            return;
        }
        (void)device->wait_idle();
        assets.clear();
        retired.clear();
        auto* const d = device->device();
        if (pipeline) {
            vkDestroyPipeline(d, pipeline, nullptr);
        }
        if (layout) {
            vkDestroyPipelineLayout(d, layout, nullptr);
        }
        if (bindless_pool) {
            vkDestroyDescriptorPool(d, bindless_pool, nullptr);
        }
        if (frame_pool) {
            vkDestroyDescriptorPool(d, frame_pool, nullptr);
        }
        if (bindless_layout) {
            vkDestroyDescriptorSetLayout(d, bindless_layout, nullptr);
        }
        if (frame_layout) {
            vkDestroyDescriptorSetLayout(d, frame_layout, nullptr);
        }
        for (auto* s : samplers) {
            if (s) {
                vkDestroySampler(d, s, nullptr);
            }
        }
        if (overhead) {
            device->release_gpu_overhead_bytes(overhead, kBindlessDescriptorBudget);
        }
    }
    Result<void> initialize(const Device& d, std::uint32_t count);
};

Result<void> PbrRenderer::Impl::initialize(const Device& d, std::uint32_t count) {
    device = &d;
    if (count == 0 || count > 2) {
        return invalid<void>("PBR requires one or two frame slots");
    }
    constexpr std::uint64_t estimate =
        static_cast<std::uint64_t>(kBindlessDescriptorBudget + 2U) * 64U;
    auto r = d.reserve_gpu_overhead_bytes(estimate, kBindlessDescriptorBudget);
    if (!r) {
        return r;
    }
    overhead = estimate;
    for (std::uint32_t i = 4096; i > 0; --i) {
        free_textures.push_back(i - 1);
        free_materials.push_back(i - 1);
    }
    auto fl =
        info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    const VkDescriptorSetLayoutBinding fb{
        0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr,
    };
    fl.bindingCount = 1;
    fl.pBindings = &fb;
    r = checked(vkCreateDescriptorSetLayout(d.device(), &fl, nullptr, &frame_layout),
                "PBR frame layout failed");
    if (!r) {
        return r;
    }
    const std::array<VkDescriptorSetLayoutBinding, 3> bindings{
        {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLER, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        },
    };
    const std::array<VkDescriptorBindingFlags, 3> flags{
        {
            0,
            0,
            static_cast<VkDescriptorBindingFlags>(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT) |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        },
    };
    auto bf = info<VkDescriptorSetLayoutBindingFlagsCreateInfo>(
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);
    bf.bindingCount = 3;
    bf.pBindingFlags = flags.data();
    auto bl =
        info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    bl.pNext = &bf;
    bl.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    bl.bindingCount = 3;
    bl.pBindings = bindings.data();
    r = checked(vkCreateDescriptorSetLayout(d.device(), &bl, nullptr, &bindless_layout),
                "PBR bindless layout failed");
    if (!r) {
        return r;
    }
    const std::array<VkDescriptorPoolSize, 3> sizes{
        {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 2},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096},
        },
    };
    auto pool = info<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool.maxSets = 1;
    pool.poolSizeCount = 3;
    pool.pPoolSizes = sizes.data();
    r = checked(vkCreateDescriptorPool(d.device(), &pool, nullptr, &bindless_pool),
                "PBR bindless pool failed");
    if (!r) {
        return r;
    }
    const std::uint32_t variable_count = 4096;
    auto variable = info<VkDescriptorSetVariableDescriptorCountAllocateInfo>(
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);
    variable.descriptorSetCount = 1;
    variable.pDescriptorCounts = &variable_count;
    auto allocate =
        info<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    allocate.pNext = &variable;
    allocate.descriptorPool = bindless_pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &bindless_layout;
    r = checked(vkAllocateDescriptorSets(d.device(), &allocate, &bindless_set),
                "PBR bindless set failed");
    if (!r) {
        return r;
    }
    const VkDescriptorPoolSize fps{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count};
    pool.flags = 0;
    pool.maxSets = count;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &fps;
    r = checked(vkCreateDescriptorPool(d.device(), &pool, nullptr, &frame_pool),
                "PBR frame pool failed");
    if (!r) {
        return r;
    }
    std::vector<VkDescriptorSetLayout> layouts(count, frame_layout);
    frame_sets.resize(count);
    draws.resize(count);
    allocate.pNext = nullptr;
    allocate.descriptorPool = frame_pool;
    allocate.descriptorSetCount = count;
    allocate.pSetLayouts = layouts.data();
    r = checked(vkAllocateDescriptorSets(d.device(), &allocate, frame_sets.data()),
                "PBR frame sets failed");
    if (!r) {
        return r;
    }
    auto storage = buffer(d, sizeof(Records), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (!storage) {
        return core::make_failure<void>(storage.error());
    }
    record_buffer = std::move(*storage);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto b = buffer(d, sizeof(Frame), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        if (!b) {
            return core::make_failure<void>(b.error());
        }
        const VkDescriptorBufferInfo bi{(*b)->handle, 0, sizeof(Frame)};
        auto w = info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        w.dstSet = frame_sets[i];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(d.device(), 1, &w, 0, nullptr);
        frames.push_back(std::move(*b));
    }
    std::array<VkDescriptorImageInfo, 2> si{};
    for (std::size_t i = 0; i < 2; ++i) {
        auto s = info<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        s.magFilter = i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        s.minFilter = s.magFilter;
        s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        s.addressModeV = s.addressModeU;
        s.addressModeW = s.addressModeU;
        s.maxLod = VK_LOD_CLAMP_NONE;
        r = checked(vkCreateSampler(d.device(), &s, nullptr, &samplers[i]), "PBR sampler failed");
        if (!r) {
            return r;
        }
        si[i].sampler = samplers[i];
    }
    const VkDescriptorBufferInfo bi{record_buffer->handle, 0, sizeof(Records)};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (auto& w : writes) {
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    }
    writes[0].dstSet = bindless_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &bi;
    writes[1].dstSet = bindless_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 2;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[1].pImageInfo = si.data();
    vkUpdateDescriptorSets(d.device(), 2, writes.data(), 0, nullptr);
    const std::array<VkDescriptorSetLayout, 2> sets{frame_layout, bindless_layout};
    const VkPushConstantRange push{
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(Push),
    };
    auto pl = info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    pl.setLayoutCount = 2;
    pl.pSetLayouts = sets.data();
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    return checked(vkCreatePipelineLayout(d.device(), &pl, nullptr, &layout),
                   "PBR pipeline layout failed");
}

PbrRenderer::PbrRenderer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PbrRenderer::~PbrRenderer() = default;
PbrRenderer::PbrRenderer(PbrRenderer&&) noexcept = default;
PbrRenderer& PbrRenderer::operator=(PbrRenderer&&) noexcept = default;
Result<PbrRenderer> PbrRenderer::create(const Device& device, std::uint32_t frames) {
    auto impl = std::make_unique<Impl>();
    auto r = impl->initialize(device, frames);
    if (!r) {
        return core::make_failure<PbrRenderer>(r.error());
    }
    return PbrRenderer(std::move(impl));
}
Result<void> PbrRenderer::collect() {
    auto r = impl_->device->wait_idle();
    if (!r) {
        return r;
    }
    impl_->retired.clear();
    return {};
}
Result<void> PbrRenderer::retire(std::string_view id) {
    auto found = impl_->assets.find(std::string(id));
    if (found == impl_->assets.end()) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::NotFound, "PBR asset not uploaded"));
    }
    impl_->retired.push_back(std::move(found->second));
    impl_->assets.erase(found);
    return {};
}
PbrStats PbrRenderer::stats() const {
    return {
        4096U - static_cast<std::uint32_t>(impl_->free_textures.size()),
        4096U - static_cast<std::uint32_t>(impl_->free_materials.size()),
        static_cast<std::uint32_t>(impl_->retired.size()),
    };
}

Result<void> PbrRenderer::upload(const asset::LoadedAsset& source) {
    auto& p = *impl_;
    const auto id = source.record.asset_id.to_string();
    if (p.assets.contains(id)) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Conflict, "PBR asset already uploaded"));
    }
    const bool needs_default = std::ranges::any_of(source.asset.meshes, [](const auto& mesh) {
        return std::ranges::any_of(
            mesh.primitives, [](const auto& primitive) { return !primitive.material.has_value(); });
    });
    const std::size_t material_count = source.asset.materials.size() + (needs_default ? 1U : 0U);
    if (source.textures.size() > p.free_textures.size() ||
        material_count > p.free_materials.size()) {
        return invalid<void>("PBR texture or material slot budget exceeded");
    }
    if (source.record.asset_id.is_nil()) {
        return invalid<void>("PBR asset ID must not be nil");
    }
    for (const auto& material : source.asset.materials) {
        const auto finite = [](float v) { return std::isfinite(v); };
        if (!std::ranges::all_of(material.base_color_factor, finite) ||
            !std::ranges::all_of(material.emissive_factor, finite) ||
            !finite(material.metallic_factor) || !finite(material.roughness_factor) ||
            !finite(material.normal_scale) || !finite(material.occlusion_strength) ||
            !finite(material.alpha_cutoff)) {
            return invalid<void>("non-finite PBR material");
        }
    }
    for (const auto& material : source.asset.materials) {
        if (std::ranges::any_of(material.base_color_factor,
                                [](float v) { return v < 0 || v > 1; }) ||
            std::ranges::any_of(material.emissive_factor, [](float v) { return v < 0; }) ||
            material.metallic_factor < 0 || material.metallic_factor > 1 ||
            material.roughness_factor < 0 || material.roughness_factor > 1 ||
            material.occlusion_strength < 0 || material.occlusion_strength > 1 ||
            material.alpha_cutoff < 0) {
            return invalid<void>("PBR material factors are out of range");
        }
    }
    for (const auto& mesh : source.asset.meshes) {
        for (const auto& primitive : mesh.primitives) {
            for (const auto& v : primitive.vertices) {
                const auto finite = [](float x) { return std::isfinite(x); };
                if (!std::ranges::all_of(v.position, finite) ||
                    !std::ranges::all_of(v.normal, finite) ||
                    !std::ranges::all_of(v.tangent, finite) ||
                    !std::ranges::all_of(v.uv0, finite)) {
                    return invalid<void>("non-finite PBR vertex");
                }
                const float length = (v.normal[0] * v.normal[0]) + (v.normal[1] * v.normal[1]) +
                                     (v.normal[2] * v.normal[2]);
                if (!std::isfinite(length) || length < 1e-12F) {
                    return invalid<void>("invalid PBR vertex normal");
                }
            }
        }
    }
    auto idle = p.device->wait_idle();
    if (!idle) {
        return idle;
    }
    auto result = std::make_unique<Asset>();
    std::map<core::Uuid, std::uint32_t> texture_ids;
    std::map<std::string, std::shared_ptr<Texture>> shared_images;
    for (const auto& t : source.textures) {
        if (texture_ids.contains(t.texture_id)) {
            return invalid<void>("duplicate cooked texture ID");
        }
        const auto key = std::to_string(t.width) + ":" + std::to_string(t.height) + ":" +
                         std::to_string(static_cast<int>(t.color_space)) + ":" +
                         core::sha256_hex(std::span<const std::byte>{
                             reinterpret_cast<const std::byte*>(t.rgba8.data()), t.rgba8.size()});
        std::shared_ptr<Texture> uploaded;
        const auto cached = shared_images.find(key);
        if (cached != shared_images.end()) {
            {
                uploaded = cached->second;
            }
        } else {
            auto image = texture(*p.device, t);
            if (!image) {
                return core::make_failure<void>(image.error());
            }
            uploaded = std::shared_ptr<Texture>(std::move(*image));
            shared_images.emplace(key, uploaded);
        }
        auto slot = take(p.free_textures);
        texture_ids.emplace(t.texture_id, slot->index);
        p.records->textures[slot->index].sampler = {
            static_cast<std::uint32_t>(t.sampler.mag_filter),
            static_cast<std::uint32_t>(t.sampler.min_filter),
            static_cast<std::uint32_t>(t.sampler.wrap_s),
            static_cast<std::uint32_t>(t.sampler.wrap_t)};
        VkDescriptorImageInfo image{};
        image.imageView = uploaded->view;
        image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        auto w = info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        w.dstSet = p.bindless_set;
        w.dstBinding = 2;
        w.dstArrayElement = slot->index;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w.pImageInfo = &image;
        vkUpdateDescriptorSets(p.device->device(), 1, &w, 0, nullptr);
        result->texture_slots.push_back(std::move(slot));
        result->textures.push_back(std::move(uploaded));
    }
    const auto image_id =
        [&](const std::optional<asset::TextureReference>& ref) -> Result<std::uint32_t> {
        if (!ref) {
            return none;
        }
        if (ref->texture >= source.asset.textures.size()) {
            return invalid<std::uint32_t>("material texture index is invalid");
        }
        auto found = texture_ids.find(source.asset.textures[ref->texture].id);
        if (found == texture_ids.end()) {
            return invalid<std::uint32_t>("cooked material image is missing");
        }
        return found->second;
    };
    for (std::size_t i = 0; i < material_count; ++i) {
        Material material;
        if (i < source.asset.materials.size()) {
            const auto& m = source.asset.materials[i];
            material.base = {m.base_color_factor[0], m.base_color_factor[1], m.base_color_factor[2],
                             m.base_color_factor[3]};
            material.factors = {m.metallic_factor, m.roughness_factor, m.normal_scale,
                                m.occlusion_strength};
            material.emissive = {m.emissive_factor[0], m.emissive_factor[1], m.emissive_factor[2],
                                 m.alpha_cutoff};
            const std::array refs{
                m.base_color_texture, m.metallic_roughness_texture, m.normal_texture,
                m.occlusion_texture,  m.emissive_texture,
            };
            std::array<std::uint32_t, 5> ids{};
            for (std::size_t j = 0; j < refs.size(); ++j) {
                auto mapped = image_id(refs[j]);
                if (!mapped) {
                    return core::make_failure<void>(mapped.error());
                }
                ids[j] = *mapped;
            }
            material.textures = {ids[0], ids[1], ids[2], ids[3]};
            material.flags = {ids[4], m.alpha_mode == asset::AlphaMode::Mask ? 1U : 0U,
                              m.double_sided ? 1U : 0U, 0};
        }
        auto slot = take(p.free_materials);
        p.records->materials[slot->index] = material;
        result->material_slots.push_back(std::move(slot));
    }
    for (const auto& mesh : source.asset.meshes) {
        std::vector<Mesh> primitives;
        for (const auto& primitive : mesh.primitives) {
            if (primitive.vertices.empty() ||
                primitive.vertices.size() > asset::kMaxVerticesPerPrimitive ||
                primitive.indices.empty() || primitive.indices.size() % 3U ||
                primitive.indices.size() > asset::kMaxIndicesPerPrimitive ||
                std::ranges::any_of(
                    primitive.indices,
                    [&](std::uint32_t i) { return i >= primitive.vertices.size(); }) ||
                (primitive.material && *primitive.material >= source.asset.materials.size())) {
                return invalid<void>("invalid PBR mesh data or material reference");
            }
            Mesh gpu;
            auto vertices = buffer(*p.device, primitive.vertices.size() * sizeof(asset::Vertex),
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            if (!vertices) {
                return core::make_failure<void>(vertices.error());
            }
            gpu.vertices = std::move(*vertices);
            auto indices = buffer(*p.device, primitive.indices.size() * sizeof(std::uint32_t),
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            if (!indices) {
                return core::make_failure<void>(indices.error());
            }
            gpu.indices = std::move(*indices);
            auto r = write(*gpu.vertices, primitive.vertices.data(),
                           primitive.vertices.size() * sizeof(asset::Vertex));
            if (!r) {
                return r;
            }
            r = write(*gpu.indices, primitive.indices.data(),
                      primitive.indices.size() * sizeof(std::uint32_t));
            if (!r) {
                return r;
            }
            gpu.count = static_cast<std::uint32_t>(primitive.indices.size());
            gpu.material =
                result->material_slots[primitive.material.value_or(source.asset.materials.size())]
                    ->index;
            primitives.push_back(std::move(gpu));
        }
        result->meshes.push_back(std::move(primitives));
    }
    const auto& nodes = source.asset.nodes;
    std::vector<glm::dmat4> matrices(nodes.size(), glm::dmat4{1});
    std::vector<int> state(nodes.size());
    std::function<Result<void>(std::size_t, std::size_t)> compose =
        [&](std::size_t i, std::size_t depth) -> Result<void> {
        if (i >= nodes.size() || depth > asset::kMaxNodeDepth || state[i] == 1) {
            return invalid<void>("invalid PBR node hierarchy");
        }
        if (state[i] == 2) {
            return {};
        }
        state[i] = 1;
        const auto& n = nodes[i];
        glm::dmat4 local =
            glm::translate(glm::dmat4{1},
                           glm::dvec3{n.translation.x, n.translation.y, n.translation.z}) *
            glm::mat4_cast(glm::normalize(
                glm::dquat{static_cast<double>(n.rotation.w), static_cast<double>(n.rotation.x),
                           static_cast<double>(n.rotation.y), static_cast<double>(n.rotation.z)}));
        local = glm::scale(local, glm::dvec3{n.scale.x, n.scale.y, n.scale.z});
        if (n.parent) {
            auto r = compose(*n.parent, depth + 1);
            if (!r) {
                return r;
            }
            local = matrices[*n.parent] * local;
        }
        matrices[i] = local;
        state[i] = 2;
        return {};
    };
    std::set<std::size_t> roots(source.asset.scene_roots.begin(), source.asset.scene_roots.end());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto r = compose(i, 0);
        if (!r) {
            return r;
        }
        auto root = i;
        std::size_t steps = 0;
        while (true) {
            const auto parent = nodes[root].parent;
            if (!parent) {
                break;
            }
            root = *parent;
            if (++steps > asset::kMaxNodeDepth) {
                return invalid<void>("invalid PBR node hierarchy");
            }
        }
        const auto mesh = nodes[i].mesh;
        if (!roots.contains(root) || !mesh) {
            continue;
        }
        if (*mesh >= result->meshes.size()) {
            return invalid<void>("PBR node references unknown mesh");
        }
        result->nodes.push_back({matrices[i], *mesh});
    }
    auto r = write(*p.record_buffer, p.records.get(), sizeof(Records));
    if (!r) {
        return r;
    }
    p.assets.emplace(id, std::move(result));
    return {};
}

Result<void> PbrRenderer::prepare(const scene::SceneView& view, const scene::WorldTransform& camera,
                                  float aspect, std::uint32_t frame) {
    auto& p = *impl_;
    if (frame >= p.frames.size()) {
        return invalid<void>("invalid PBR frame slot");
    }
    Frame uniforms;
    bool light_found = false;
    for (auto entity : view.entities()) {
        if (view.scene().has<scene::DirectionalLight>(entity)) {
            const auto& light = view.scene().component<scene::DirectionalLight>(entity);
            if (!light.enabled) {
                continue;
            }
            if (light_found) {
                return invalid<void>("M3 PBR supports only one enabled DirectionalLight");
            }
            light_found = true;
            const glm::vec3 direction{light.direction.x, light.direction.y, light.direction.z};
            if (!std::isfinite(glm::dot(direction, direction)) ||
                glm::dot(direction, direction) < 1e-12F) {
                return invalid<void>("directional light direction must be finite and nonzero");
            }
            if (!std::isfinite(light.intensity) || light.intensity < 0 ||
                !std::isfinite(light.color.x) || !std::isfinite(light.color.y) ||
                !std::isfinite(light.color.z) || light.color.x < 0 || light.color.y < 0 ||
                light.color.z < 0) {
                return invalid<void>("directional light radiance must be finite and nonnegative");
            }
            uniforms.direction = glm::vec4{glm::normalize(direction), 0};
            uniforms.radiance = {light.color.x * light.intensity, light.color.y * light.intensity,
                                 light.color.z * light.intensity, 0};
        }
    }
    auto r = write(*p.frames[frame], &uniforms, sizeof(uniforms));
    if (!r) {
        return r;
    }
    const auto camera_entity = view.active_camera();
    if (!camera_entity) {
        return invalid<void>("PBR scene has no camera");
    }
    const auto& component = view.camera(*camera_entity);
    if (!std::isfinite(aspect) || aspect <= 0 || !std::isfinite(component.fovYRadians) ||
        component.fovYRadians <= 0 || component.fovYRadians >= std::numbers::pi_v<float> ||
        !std::isfinite(component.near) || !std::isfinite(component.far) || component.near <= 0 ||
        component.far <= component.near) {
        return invalid<void>("invalid PBR camera projection");
    }
    Camera projection;
    projection.vertical_fov_radians = component.fovYRadians;
    projection.near_plane = component.near;
    projection.far_plane = component.far;
    const auto vp = projection.projection(aspect) * camera_view_from_world(camera);
    auto& draws = p.draws[frame];
    draws.clear();
    for (auto entity : view.entities()) {
        if (!view.scene().has<scene::AssetInstance>(entity)) {
            continue;
        }
        const auto& id = view.scene().component<scene::AssetInstance>(entity).asset_id;
        auto found = p.assets.find(id);
        if (found == p.assets.end()) {
            return core::make_failure<void>(core::make_error(core::ErrorCode::NotFound,
                                                             "scene asset has not been uploaded",
                                                             {}, {{"assetId", id}}));
        }
        const auto root = view.world_transform(entity);
        auto model = glm::translate(glm::dmat4{1},
                                    glm::dvec3{root.position.x, root.position.y, root.position.z}) *
                     glm::mat4_cast(glm::normalize(glm::dquat{root.rotation.w, root.rotation.x,
                                                              root.rotation.y, root.rotation.z}));
        model = glm::scale(model, glm::dvec3{root.scale.x, root.scale.y, root.scale.z});
        for (const auto& node : found->second->nodes) {
            auto world = model * node.transform;
            world[3] -= glm::dvec4{camera.position.x, camera.position.y, camera.position.z, 0};
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 4; ++row) {
                    if (!std::isfinite(world[column][row]) ||
                        std::abs(world[column][row]) >
                            static_cast<double>(std::numeric_limits<float>::max())) {
                        return invalid<void>("PBR camera-relative transform exceeds Float32 range");
                    }
                }
            }
            if (!std::isfinite(glm::determinant(glm::dmat3{world})) ||
                std::abs(glm::determinant(glm::dmat3{world})) < 1e-16) {
                return invalid<void>("PBR transform must be finite and invertible");
            }
            for (const auto& mesh : found->second->meshes[node.mesh]) {
                Push push;
                push.vp = vp;
                push.material = mesh.material;
                push.encode_srgb =
                    p.color == VK_FORMAT_R8G8B8A8_SRGB || p.color == VK_FORMAT_B8G8R8A8_SRGB ? 0U
                                                                                             : 1U;
                push.row0 = {world[0][0], world[1][0], world[2][0], world[3][0]};
                push.row1 = {world[0][1], world[1][1], world[2][1], world[3][1]};
                push.row2 = {world[0][2], world[1][2], world[2][2], world[3][2]};
                draws.push_back({&mesh, push});
            }
        }
    }
    return {};
}
void PbrRenderer::draw(VkCommandBuffer cmd, std::uint32_t frame) const {
    const auto& p = *impl_;
    if (p.draws[frame].empty()) {
        return;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline);
    const std::array sets{p.frame_sets[frame], p.bindless_set};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 0, 2, sets.data(), 0,
                            nullptr);
    for (const auto& draw : p.draws[frame]) {
        vkCmdPushConstants(cmd, p.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(Push), &draw.push);
        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &draw.mesh->vertices->handle, &offset);
        vkCmdBindIndexBuffer(cmd, draw.mesh->indices->handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, draw.mesh->count, 1, 0, 0, 0);
    }
}

core::Result<void> PbrRenderer::set_format(VkFormat color, VkFormat depth) {
    if (impl_->pipeline && impl_->color == color && impl_->depth == depth) {
        return {};
    }
    auto idle = impl_->device->wait_idle();
    if (!idle) {
        return idle;
    }
    if (impl_->pipeline) {
        vkDestroyPipeline(impl_->device->device(), impl_->pipeline, nullptr);
        impl_->pipeline = VK_NULL_HANDLE;
    }
    const Device* device = impl_->device;
    const VkPipelineLayout pipeline_layout = impl_->layout;
    VkPipeline& pipeline = impl_->pipeline;
    const VkFormat kColorFormat = color;
    const VkFormat kDepthFormat = depth;
    auto vertex_module =
        create_shader_module(device->device(), shaders::kPbrVertex, shaders::kPbrVertexSize);
    if (!vertex_module) {
        return core::make_failure<void>(vertex_module.error());
    }
    auto fragment_module =
        create_shader_module(device->device(), shaders::kPbrFragment, shaders::kPbrFragmentSize);
    if (!fragment_module) {
        vkDestroyShaderModule(device->device(), *vertex_module, nullptr);
        return core::make_failure<void>(fragment_module.error());
    }

    VkResult result = VK_SUCCESS;
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

    VkVertexInputBindingDescription binding{0, sizeof(asset::Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array<VkVertexInputAttributeDescription, 4> attributes{
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(asset::Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(asset::Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(asset::Vertex, tangent)},
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(asset::Vertex, uv0)},
        },
    };

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
    impl_->color = color;
    impl_->depth = depth;
    return {};
}

} // namespace engine::render
