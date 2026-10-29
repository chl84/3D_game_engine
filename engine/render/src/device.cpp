#include <engine/render/device.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine::render {
namespace {

constexpr std::string_view kValidationLayer{"VK_LAYER_KHRONOS_validation"};
constexpr std::uint64_t kMaxValidationMessages{64};

std::atomic<int> g_active_devices{0};

struct ValidationState {
    std::atomic<std::uint64_t> errors{0};
    std::atomic<std::uint64_t> warnings{0};
    std::mutex mutex;
    std::deque<std::string> messages;
};

VKAPI_ATTR VkBool32 VKAPI_CALL
validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                    VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
    auto* state = static_cast<ValidationState*>(user_data);
    if (state == nullptr) {
        return VK_FALSE;
    }

    const bool is_error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    const bool is_warning = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;
    if (!is_error && !is_warning) {
        return VK_FALSE;
    }

    if (is_error) {
        state->errors.fetch_add(1, std::memory_order_relaxed);
    } else {
        state->warnings.fetch_add(1, std::memory_order_relaxed);
    }

    std::string message = is_error ? "error: " : "warning: ";
    if (callback_data != nullptr && callback_data->pMessage != nullptr) {
        message += callback_data->pMessage;
    } else {
        message += "validation message without text";
    }

    const std::scoped_lock lock(state->mutex);
    if (state->messages.size() >= kMaxValidationMessages) {
        state->messages.pop_front();
    }
    state->messages.push_back(std::move(message));
    return VK_FALSE;
}

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
        return "unknown VkResult";
    }
}

core::Error vulkan_error(std::string message, std::string stage, VkResult result) {
    return core::make_error(core::ErrorCode::Precondition, std::move(message),
                            std::string("Vulkan returned ") + vk_result_name(result),
                            {{"stage", std::move(stage)}, {"vkResult", static_cast<int>(result)}});
}

[[nodiscard]] bool is_vulkan_13(std::uint32_t version) {
    return VK_API_VERSION_MAJOR(version) > 1 ||
           (VK_API_VERSION_MAJOR(version) == 1 && VK_API_VERSION_MINOR(version) >= 3);
}

[[nodiscard]] bool has_instance_layer(std::string_view name) {
    std::uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }
    std::vector<VkLayerProperties> layers(count);
    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
        return false;
    }
    return std::ranges::any_of(
        layers, [name](const VkLayerProperties& layer) { return name == layer.layerName; });
}

[[nodiscard]] bool has_instance_extension(std::string_view name) {
    std::uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS ||
        count == 0) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return false;
    }
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return name == extension.extensionName;
    });
}

[[nodiscard]] bool has_device_extension(VkPhysicalDevice device, std::string_view name) {
    std::uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS ||
        count == 0) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) !=
        VK_SUCCESS) {
        return false;
    }
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return name == extension.extensionName;
    });
}

void add_unique(std::vector<std::string>& names, std::string name) {
    if (std::ranges::find(names, name) == names.end()) {
        names.push_back(std::move(name));
    }
}

[[nodiscard]] bool supports_vulkan_13_features(VkPhysicalDevice device) {
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(device, &features);
    return features13.synchronization2 == VK_TRUE && features13.dynamicRendering == VK_TRUE;
}

[[nodiscard]] core::Result<QueueFamilies> find_queue_families(VkPhysicalDevice device,
                                                              VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (count == 0) {
        return core::make_failure<QueueFamilies>(core::make_error(
            core::ErrorCode::Unsupported, "the physical device has no queue families", {},
            {{"stage", "queue"}}));
    }

    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    const bool needs_present = surface != VK_NULL_HANDLE;
    const auto supports_present = [device, surface](std::uint32_t index) {
        VkBool32 supported = VK_FALSE;
        const VkResult result =
            vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &supported);
        return result == VK_SUCCESS && supported == VK_TRUE;
    };

    for (std::uint32_t index = 0; index < count; ++index) {
        const bool graphics = (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (graphics && (!needs_present || supports_present(index))) {
            return QueueFamilies{index, index, needs_present};
        }
    }

    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;
    for (std::uint32_t index = 0; index < count; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && !graphics) {
            graphics = index;
        }
        if (needs_present && supports_present(index) && !present) {
            present = index;
        }
    }

    if (!graphics) {
        return core::make_failure<QueueFamilies>(core::make_error(
            core::ErrorCode::Unsupported, "the physical device has no graphics queue", {},
            {{"stage", "queue"}}));
    }
    if (needs_present && !present) {
        return core::make_failure<QueueFamilies>(core::make_error(
            core::ErrorCode::Unsupported, "the physical device cannot present to the surface", {},
            {{"stage", "queue"}}));
    }

    return QueueFamilies{*graphics, present.value_or(*graphics), needs_present};
}

[[nodiscard]] bool supports_swapchain(VkPhysicalDevice device, VkSurfaceKHR surface) {
    if (!has_device_extension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        return false;
    }

    std::uint32_t format_count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr) !=
            VK_SUCCESS ||
        format_count == 0) {
        return false;
    }

    std::uint32_t mode_count = 0;
    return vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count, nullptr) ==
               VK_SUCCESS &&
           mode_count > 0;
}

[[nodiscard]] int device_score(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 1;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return 0;
    }
}

struct Selection {
    VkPhysicalDevice device{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties properties{};
    QueueFamilies queues{};
};

[[nodiscard]] core::Result<Selection> select_physical_device(VkInstance instance,
                                                             VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS) {
        return core::make_failure<Selection>(vulkan_error(
            "could not enumerate the Vulkan physical devices", "physical-device", result));
    }
    if (count == 0) {
        return core::make_failure<Selection>(
            core::make_error(core::ErrorCode::Unsupported, "no Vulkan physical device is available",
                             "Install a Vulkan 1.3 driver.", {{"stage", "physical-device"}}));
    }

    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (result != VK_SUCCESS) {
        return core::make_failure<Selection>(vulkan_error(
            "could not enumerate the Vulkan physical devices", "physical-device", result));
    }

    nlohmann::json rejected = nlohmann::json::array();
    Selection best;
    int best_score = -1;

    for (const VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        const std::string name{properties.deviceName};

        if (!is_vulkan_13(properties.apiVersion)) {
            rejected.push_back({{"device", name}, {"reason", "Vulkan 1.3 is not supported"}});
            continue;
        }

        const auto queues = find_queue_families(device, surface);
        if (!queues) {
            rejected.push_back({{"device", name}, {"reason", queues.error().message}});
            continue;
        }

        if (!supports_vulkan_13_features(device)) {
            rejected.push_back({
                {"device", name},
                {"reason", "Synchronization2 or Dynamic Rendering is not supported"},
            });
            continue;
        }

        if (surface != VK_NULL_HANDLE && !supports_swapchain(device, surface)) {
            rejected.push_back(
                {{"device", name}, {"reason", "the swapchain extension is not supported"}});
            continue;
        }

        const int score = device_score(properties.deviceType);
        if (score > best_score) {
            best_score = score;
            best.device = device;
            best.properties = properties;
            best.queues = *queues;
        }
    }

    if (best.device == VK_NULL_HANDLE) {
        return core::make_failure<Selection>(core::make_error(
            core::ErrorCode::Unsupported, "no suitable Vulkan 1.3 device is available",
            "A Vulkan 1.3 device with Synchronization2 and Dynamic Rendering is required.",
            {{"stage", "physical-device"}, {"rejected", rejected}}));
    }

    return best;
}

} // namespace

struct Device::Impl {
    VkInstance instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT messenger{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue graphics_queue{VK_NULL_HANDLE};
    VkQueue present_queue{VK_NULL_HANDLE};
    VmaAllocator allocator{nullptr};
    VkPhysicalDeviceProperties properties{};
    QueueFamilies queues{};
    bool validation_enabled{false};
    bool loader_initialized{false};
    std::unique_ptr<ValidationState> validation{std::make_unique<ValidationState>()};

    ~Impl();

    [[nodiscard]] core::Result<void> initialize(const DeviceOptions& options);
};

Device::Impl::~Impl() {
    if (allocator != nullptr) {
        vmaDestroyAllocator(allocator);
    }
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }
    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (messenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE &&
        vkDestroyDebugUtilsMessengerEXT != nullptr) {
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
    if (loader_initialized) {
        volkFinalize();
    }
    g_active_devices.store(0);
}

core::Result<void> Device::Impl::initialize(const DeviceOptions& options) {
    if (volkInitialize() != VK_SUCCESS) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Unsupported, "the Vulkan loader is not available",
                             "Install the Vulkan loader or run on a machine with Vulkan support.",
                             {{"stage", "loader"}}));
    }
    loader_initialized = true;

    std::uint32_t loader_version = 0;
    if (vkEnumerateInstanceVersion == nullptr ||
        vkEnumerateInstanceVersion(&loader_version) != VK_SUCCESS) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Unsupported, "could not query the Vulkan loader version",
            "Install a Vulkan 1.3 loader.", {{"stage", "loader-version"}}));
    }
    if (!is_vulkan_13(loader_version)) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Unsupported, "the Vulkan loader is older than 1.3",
                             "Install a Vulkan 1.3 loader.",
                             {{"stage", "loader-version"}, {"version", loader_version}}));
    }

    const bool layer_available = has_instance_layer(kValidationLayer);
    const bool debug_utils_available = has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    switch (options.validation) {
    case ValidationMode::Disabled:
        validation_enabled = false;
        break;
    case ValidationMode::Auto:
        validation_enabled = layer_available && debug_utils_available;
        break;
    case ValidationMode::Required:
        if (!layer_available) {
            return core::make_failure<void>(core::make_error(
                core::ErrorCode::Unsupported, "VK_LAYER_KHRONOS_validation is not available",
                "Install the Vulkan validation layers.", {{"stage", "validation"}}));
        }
        if (!debug_utils_available) {
            return core::make_failure<void>(core::make_error(
                core::ErrorCode::Unsupported, "VK_EXT_debug_utils is not available",
                "Install a Vulkan 1.3 loader with debug utilities.", {{"stage", "validation"}}));
        }
        validation_enabled = true;
        break;
    }

    std::vector<std::string> extension_names;
    if (options.window != nullptr) {
        auto window_extensions = options.window->vulkan_instance_extensions();
        if (!window_extensions) {
            return core::make_failure<void>(window_extensions.error());
        }
        for (const std::string& name : *window_extensions) {
            add_unique(extension_names, name);
        }
    }
    if (validation_enabled) {
        add_unique(extension_names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    for (const std::string& name : extension_names) {
        if (!has_instance_extension(name)) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::Unsupported,
                                 "the required Vulkan instance extension is not available",
                                 "Update the Vulkan loader or the graphics driver.",
                                 {{"stage", "instance-extension"}, {"extension", name}}));
        }
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (validation_enabled) {
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) |
                                            static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);
        debug_create_info.messageType = static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) |
                                        static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) |
                                        static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
        debug_create_info.pfnUserCallback = validation_callback;
        debug_create_info.pUserData = validation.get();
    }

    std::vector<const char*> extension_pointers;
    extension_pointers.reserve(extension_names.size());
    for (const std::string& name : extension_names) {
        extension_pointers.push_back(name.c_str());
    }

    const char* layer_name = "VK_LAYER_KHRONOS_validation";

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = options.application_name.c_str();
    application_info.applicationVersion = options.application_version;
    application_info.pEngineName = "engine";
    application_info.engineVersion = 0;
    application_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &application_info;
    instance_create_info.enabledExtensionCount =
        static_cast<std::uint32_t>(extension_pointers.size());
    instance_create_info.ppEnabledExtensionNames =
        extension_pointers.empty() ? nullptr : extension_pointers.data();
    if (validation_enabled) {
        instance_create_info.enabledLayerCount = 1;
        instance_create_info.ppEnabledLayerNames = &layer_name;
        instance_create_info.pNext = &debug_create_info;
    }

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            vulkan_error("could not create the Vulkan instance", "instance", result));
    }
    volkLoadInstance(instance);

    if (validation_enabled) {
        const auto create_messenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create_messenger == nullptr) {
            return core::make_failure<void>(core::make_error(
                core::ErrorCode::Unsupported, "the debug messenger is not available",
                "Install a Vulkan 1.3 loader with debug utilities.", {{"stage", "validation"}}));
        }
        result = create_messenger(instance, &debug_create_info, nullptr, &messenger);
        if (result != VK_SUCCESS) {
            return core::make_failure<void>(
                vulkan_error("could not create the debug messenger", "validation", result));
        }
    }

    if (options.window != nullptr) {
        auto surface_result = options.window->create_vulkan_surface(instance);
        if (!surface_result) {
            return core::make_failure<void>(surface_result.error());
        }
        surface = *surface_result;
    }

    auto selection = select_physical_device(instance, surface);
    if (!selection) {
        return core::make_failure<void>(selection.error());
    }
    physical_device = selection->device;
    properties = selection->properties;
    queues = selection->queues;

    std::vector<std::uint32_t> families{queues.graphics};
    if (queues.present != queues.graphics) {
        families.push_back(queues.present);
    }

    std::array<float, 2> priorities{1.0F, 1.0F};
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(families.size());
    for (std::size_t index = 0; index < families.size(); ++index) {
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = families[index];
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = priorities.data() + index;
        queue_infos.push_back(queue_info);
    }

    std::vector<const char*> device_extensions;
    if (surface != VK_NULL_HANDLE) {
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = &features13;
    device_create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
    device_create_info.pQueueCreateInfos = queue_infos.data();
    device_create_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames =
        device_extensions.empty() ? nullptr : device_extensions.data();

    result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            vulkan_error("could not create the Vulkan device", "device", result));
    }
    volkLoadDevice(device);

    vkGetDeviceQueue(device, queues.graphics, 0, &graphics_queue);
    vkGetDeviceQueue(device, queues.present, 0, &present_queue);

    VmaVulkanFunctions vulkan_functions{};
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.instance = instance;
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator_info.pVulkanFunctions = &vulkan_functions;

    result = vmaImportVulkanFunctionsFromVolk(&allocator_info, &vulkan_functions);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            vulkan_error("could not import the Vulkan functions for VMA", "allocator", result));
    }

    result = vmaCreateAllocator(&allocator_info, &allocator);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            vulkan_error("could not create the VMA allocator", "allocator", result));
    }

    return {};
}

core::Result<Device> Device::create(const DeviceOptions& options) {
    if (g_active_devices.exchange(1, std::memory_order_acq_rel) != 0) {
        return core::make_failure<Device>(core::make_error(
            core::ErrorCode::Precondition, "only one render device can exist per process", {},
            {{"stage", "device-lifetime"}}));
    }

    auto impl = std::make_unique<Impl>();
    auto initialized = impl->initialize(options);
    if (!initialized) {
        const core::Error& error = initialized.error();
        impl.reset();
        return core::make_failure<Device>(error);
    }

    return Device(std::move(impl));
}

Device::Device(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Device::Device(Device&&) noexcept = default;

Device& Device::operator=(Device&&) noexcept = default;

Device::~Device() = default;

VkInstance Device::instance() const noexcept {
    return impl_->instance;
}

VkPhysicalDevice Device::physical_device() const noexcept {
    return impl_->physical_device;
}

VkDevice Device::device() const noexcept {
    return impl_->device;
}

VkQueue Device::graphics_queue() const noexcept {
    return impl_->graphics_queue;
}

VkQueue Device::present_queue() const noexcept {
    return impl_->present_queue;
}

VmaAllocator Device::allocator() const noexcept {
    return impl_->allocator;
}

const QueueFamilies& Device::queue_families() const noexcept {
    return impl_->queues;
}

const VkPhysicalDeviceProperties& Device::properties() const noexcept {
    return impl_->properties;
}

std::string Device::device_name() const {
    return std::string{impl_->properties.deviceName};
}

std::uint32_t Device::api_version() const noexcept {
    return impl_->properties.apiVersion;
}

bool Device::has_surface() const noexcept {
    return impl_->surface != VK_NULL_HANDLE;
}

bool Device::validation_enabled() const noexcept {
    return impl_->validation_enabled;
}

ValidationReport Device::validation_report() const {
    ValidationReport report;
    if (impl_->validation == nullptr) {
        return report;
    }

    report.errors = impl_->validation->errors.load(std::memory_order_relaxed);
    report.warnings = impl_->validation->warnings.load(std::memory_order_relaxed);
    const std::scoped_lock lock(impl_->validation->mutex);
    report.messages.assign(impl_->validation->messages.begin(), impl_->validation->messages.end());
    return report;
}

core::Result<void> Device::wait_idle() const {
    if (impl_->device == VK_NULL_HANDLE) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Precondition,
                                                         "the Vulkan device is not valid", {},
                                                         {{"stage", "device"}}));
    }

    const VkResult result = vkDeviceWaitIdle(impl_->device);
    if (result != VK_SUCCESS) {
        return core::make_failure<void>(
            vulkan_error("could not wait for the Vulkan device", "device", result));
    }
    return {};
}

} // namespace engine::render
