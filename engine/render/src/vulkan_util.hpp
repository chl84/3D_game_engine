#pragma once

#include <engine/core/error.hpp>
#include <engine/render/device.hpp>

#include <volk.h>

#include <string>

namespace engine::render::detail {

// Applies the total engine budget to legacy targets/buffers as well as PBR.
[[nodiscard]] VkResult create_budgeted_buffer(const Device& device,
                                              const VkBufferCreateInfo* create,
                                              const VmaAllocationCreateInfo* alloc,
                                              VkBuffer* buffer, VmaAllocation* allocation,
                                              VmaAllocationInfo* details);
[[nodiscard]] VkResult create_budgeted_image(const Device& device, const VkImageCreateInfo* create,
                                             const VmaAllocationCreateInfo* alloc, VkImage* image,
                                             VmaAllocation* allocation, VmaAllocationInfo* details);

[[nodiscard]] const char* vk_result_name(VkResult result);

[[nodiscard]] core::Error vulkan_error(std::string message, std::string stage, VkResult result);

} // namespace engine::render::detail
