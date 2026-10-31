#include "vulkan_util.hpp"

#include <utility>

namespace engine::render::detail {
namespace {

constexpr const char* kUnknownResult = "unknown VkResult";

} // namespace

const char* vk_result_name(VkResult result) {
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    default:
        return kUnknownResult;
    }
}

core::Error vulkan_error(std::string message, std::string stage, VkResult result) {
    return core::make_error(core::ErrorCode::Precondition, std::move(message),
                            std::string("Vulkan returned ") + vk_result_name(result),
                            {{"stage", std::move(stage)}, {"vkResult", static_cast<int>(result)}});
}

VkResult create_budgeted_buffer(const Device& device, const VkBufferCreateInfo* create,
                                const VmaAllocationCreateInfo* alloc, VkBuffer* buffer,
                                VmaAllocation* allocation, VmaAllocationInfo* details) {
    const auto result =
        vmaCreateBuffer(device.allocator(), create, alloc, buffer, allocation, details);
    if (result == VK_SUCCESS && device.gpu_memory_bytes() > kTotalGpuMemoryBudgetBytes) {
        vmaDestroyBuffer(device.allocator(), *buffer, *allocation);
        *buffer = VK_NULL_HANDLE;
        *allocation = nullptr;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return result;
}
VkResult create_budgeted_image(const Device& device, const VkImageCreateInfo* create,
                               const VmaAllocationCreateInfo* alloc, VkImage* image,
                               VmaAllocation* allocation, VmaAllocationInfo* details) {
    const auto result =
        vmaCreateImage(device.allocator(), create, alloc, image, allocation, details);
    if (result == VK_SUCCESS && device.gpu_memory_bytes() > kTotalGpuMemoryBudgetBytes) {
        vmaDestroyImage(device.allocator(), *image, *allocation);
        *image = VK_NULL_HANDLE;
        *allocation = nullptr;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return result;
}

} // namespace engine::render::detail
