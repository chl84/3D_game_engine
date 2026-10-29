#include <engine/render/renderer.hpp>

#include "vulkan_util.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace engine::render {
namespace {

constexpr std::size_t kFramesInFlight = 2;

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
                      VkAccessFlags2 destination_access) {
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
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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

    const Device* device{nullptr};
    const platform::Window* window{nullptr};
    SwapchainOptions options;
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkSemaphore> render_finished;
    std::array<Frame, kFramesInFlight> frames{};
    std::size_t frame_index{0};
    std::uint64_t presented_frames{0};

    ~Impl();

    [[nodiscard]] core::Result<void> initialize(const Device& render_device,
                                                const platform::Window& render_window,
                                                const SwapchainOptions& swapchain_options);
    [[nodiscard]] core::Result<void> create_frame_resources();
    void destroy_frame_resources();
    [[nodiscard]] core::Result<void> create_swapchain();
    [[nodiscard]] core::Result<void> recreate_swapchain();
    void destroy_swapchain();
    [[nodiscard]] core::Result<void> record_clear(std::uint32_t image_index);
};

Renderer::Impl::~Impl() {
    if (device != nullptr && device->device() != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device->device());
    }
    destroy_swapchain();
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

    auto frame_resources = create_frame_resources();
    if (!frame_resources) {
        return frame_resources;
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
    const VkDevice handle = device->device();
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

core::Result<void> Renderer::Impl::record_clear(std::uint32_t image_index) {
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

    transition_image(
        frame.command_buffer, images[image_index], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = image_views[image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue = clear_color();

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = VkOffset2D{0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vkCmdBeginRendering(frame.command_buffer, &rendering_info);
    vkCmdEndRendering(frame.command_buffer);

    transition_image(frame.command_buffer, images[image_index],
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);

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

core::Result<bool> Renderer::draw_frame() {
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
    if (std::cmp_not_equal(impl_->extent.width, pixel_width) ||
        std::cmp_not_equal(impl_->extent.height, pixel_height)) {
        auto recreated = impl_->recreate_swapchain();
        if (!recreated) {
            return core::make_failure<bool>(recreated.error());
        }
        if (impl_->swapchain == VK_NULL_HANDLE) {
            return false;
        }
    }

    Impl::Frame& frame = impl_->frames[impl_->frame_index];
    VkResult result = vkWaitForFences(impl_->device->device(), 1, &frame.in_flight, VK_TRUE,
                                      std::numeric_limits<std::uint64_t>::max());
    if (result != VK_SUCCESS) {
        return core::make_failure<bool>(
            detail::vulkan_error("could not wait for the frame fence", "frame", result));
    }

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

    auto recorded = impl_->record_clear(image_index);
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

} // namespace engine::render
