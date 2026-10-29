#include <engine/render/device.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

[[nodiscard]] bool environment_unavailable(const engine::core::Error& error) {
    if (!error.details.is_object()) {
        return false;
    }
    const std::string stage = error.details.value("stage", "");
    return stage == "loader" || stage == "physical-device" || stage == "validation";
}

[[nodiscard]] engine::render::DeviceOptions
device_options(engine::render::ValidationMode validation) {
    engine::render::DeviceOptions options;
    options.application_name = "engine render tests";
    options.validation = validation;
    return options;
}

[[nodiscard]] std::string report_text(const engine::render::ValidationReport& report) {
    std::string text;
    for (const std::string& message : report.messages) {
        if (!text.empty()) {
            text += '\n';
        }
        text += message;
    }
    return text;
}

} // namespace

TEST_CASE("headless Vulkan device initializes", "[render]") {
    auto device =
        engine::render::Device::create(device_options(engine::render::ValidationMode::Auto));
    if (!device) {
        if (environment_unavailable(device.error())) {
            SKIP("Vulkan environment is unavailable: " + device.error().message);
        }
        INFO("device creation failed: " << device.error().message << " " << device.error().hint);
    }
    REQUIRE(device.has_value());

    CHECK(device->instance() != VK_NULL_HANDLE);
    CHECK(device->physical_device() != VK_NULL_HANDLE);
    CHECK(device->device() != VK_NULL_HANDLE);
    CHECK(device->graphics_queue() != VK_NULL_HANDLE);
    CHECK(device->present_queue() != VK_NULL_HANDLE);
    CHECK(device->allocator() != nullptr);
    CHECK(device->api_version() >= VK_API_VERSION_1_3);
    CHECK_FALSE(device->device_name().empty());
    CHECK_FALSE(device->has_surface());
    CHECK_FALSE(device->queue_families().presentation);
    CHECK(device->wait_idle().has_value());
}

TEST_CASE("required validation layers report no errors", "[render][validation]") {
    auto device =
        engine::render::Device::create(device_options(engine::render::ValidationMode::Required));
    if (!device) {
        if (environment_unavailable(device.error())) {
            SKIP("Vulkan environment is unavailable: " + device.error().message);
        }
        INFO("device creation failed: " << device.error().message << " " << device.error().hint);
    }
    REQUIRE(device.has_value());
    CHECK(device->validation_enabled());

    const engine::render::ValidationReport report = device->validation_report();
    INFO(report_text(report));
    CHECK(report.errors == 0);
}

TEST_CASE("only one render device can exist per process", "[render]") {
    auto first =
        engine::render::Device::create(device_options(engine::render::ValidationMode::Disabled));
    if (!first) {
        if (environment_unavailable(first.error())) {
            SKIP("Vulkan environment is unavailable: " + first.error().message);
        }
        INFO("device creation failed: " << first.error().message << " " << first.error().hint);
    }
    REQUIRE(first.has_value());

    auto second =
        engine::render::Device::create(device_options(engine::render::ValidationMode::Disabled));
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().details.value("stage", "") == "device-lifetime");
}

TEST_CASE("render devices can be created sequentially", "[render]") {
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto device = engine::render::Device::create(
            device_options(engine::render::ValidationMode::Disabled));
        if (!device) {
            const std::string message = device.error().message + " " + device.error().hint +
                                        " (stage " +
                                        device.error().details.value("stage", "?") + ")";
            if (environment_unavailable(device.error())) {
                SKIP("Vulkan environment is unavailable: " + message);
            }
            FAIL("attempt " << attempt << " failed: " << message);
        }
        REQUIRE(device.has_value());
        CHECK(device->wait_idle().has_value());
    }
}
