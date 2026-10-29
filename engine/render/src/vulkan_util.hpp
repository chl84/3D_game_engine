#pragma once

#include <engine/core/error.hpp>

#include <volk.h>

#include <string>

namespace engine::render::detail {

[[nodiscard]] const char* vk_result_name(VkResult result);

[[nodiscard]] core::Error vulkan_error(std::string message, std::string stage, VkResult result);

} // namespace engine::render::detail
