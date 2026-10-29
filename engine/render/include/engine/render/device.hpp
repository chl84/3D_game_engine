#pragma once

#include <engine/core/error.hpp>
#include <engine/platform/window.hpp>

#include <volk.h>

#include <vk_mem_alloc.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace engine::render {

enum class ValidationMode : std::uint8_t {
    Disabled,
    Auto,
    Required,
};

[[nodiscard]] constexpr ValidationMode default_validation_mode() noexcept {
#ifdef NDEBUG
    return ValidationMode::Disabled;
#else
    return ValidationMode::Auto;
#endif
}

struct DeviceOptions {
    std::string application_name{"engine"};
    std::uint32_t application_version{0};
    ValidationMode validation{default_validation_mode()};
    // When set, the device creates a surface and requires a present queue.
    // The window must outlive the surface, which is owned by the device.
    const platform::Window* window{nullptr};
};

struct ValidationReport {
    std::uint64_t errors{0};
    std::uint64_t warnings{0};
    std::vector<std::string> messages;
};

struct QueueFamilies {
    std::uint32_t graphics{0};
    std::uint32_t present{0};
    bool presentation{false};
};

// A Vulkan 1.3 device with Synchronization2, Dynamic Rendering and VMA.
//
// volk keeps process-wide function tables, so only one Device may exist at a
// time. Device creation is not thread-safe; create it before starting worker
// threads.
class Device {
public:
    [[nodiscard]] static core::Result<Device> create(const DeviceOptions& options);

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;
    ~Device();

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] VmaAllocator allocator() const noexcept;
    [[nodiscard]] const QueueFamilies& queue_families() const noexcept;
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const noexcept;
    [[nodiscard]] std::string device_name() const;
    [[nodiscard]] std::uint32_t api_version() const noexcept;
    [[nodiscard]] bool has_surface() const noexcept;
    [[nodiscard]] bool validation_enabled() const noexcept;
    [[nodiscard]] ValidationReport validation_report() const;

    [[nodiscard]] core::Result<void> wait_idle() const;

private:
    struct Impl;

    explicit Device(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace engine::render
