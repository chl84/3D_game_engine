#include <catch2/catch_test_macros.hpp>

#include <CLI/CLI.hpp>
#include <SDL3/SDL_version.h>
#include <entt/entt.hpp>
#include <fastgltf/core.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tl/expected.hpp>
#include <volk.h>

#include <vk_mem_alloc.h>

#include <string>

TEST_CASE("third-party dependencies compile and link", "[dependencies]") {
    nlohmann::json document = {{"engine", "smoke"}};
    REQUIRE(document.at("engine") == "smoke");

    CLI::App app;
    bool flag = false;
    app.add_flag("--smoke", flag);
    REQUIRE_FALSE(app.get_options().empty());

    tl::expected<int, std::string> value = 42;
    REQUIRE(value.has_value());
    REQUIRE(value.value() == 42);

    spdlog::set_level(spdlog::level::debug);
    REQUIRE(spdlog::get_level() == spdlog::level::debug);
    spdlog::set_level(spdlog::level::info);

    entt::registry registry;
    const entt::entity entity = registry.create();
    registry.emplace<int>(entity, 42);
    REQUIRE(registry.get<int>(entity) == 42);

    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema({
        {"type", "object"},
        {"required", nlohmann::json::array({"name"})},
        {"properties", {{"name", {{"type", "string"}}}}},
    });
    REQUIRE_NOTHROW(validator.validate(nlohmann::json{{"name", "smoke"}}));

    STATIC_REQUIRE(SDL_MAJOR_VERSION == 3);
    const glm::vec3 vector{1.0F, 2.0F, 3.0F};
    REQUIRE(glm::length(vector) > 3.0F);

    VmaAllocatorCreateInfo allocator_info{};
    REQUIRE(allocator_info.instance == VK_NULL_HANDLE);

    fastgltf::Parser parser;
    (void)parser;

    ImGuiContext* imgui_context = ImGui::CreateContext();
    REQUIRE(imgui_context != nullptr);
    REQUIRE(ImGui::GetCurrentContext() == imgui_context);
    ImGui::DestroyContext(imgui_context);
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
}
