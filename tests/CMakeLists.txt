if(EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
    set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
    set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(engine_core_tests
    unit/command_test.cpp
    unit/doctor_support_test.cpp
    unit/config_test.cpp
    unit/error_test.cpp
    unit/log_test.cpp
    unit/path_sandbox_test.cpp
    unit/project_test.cpp
    unit/reflect_test.cpp
    unit/scene_test.cpp
    unit/scene_view_test.cpp
    unit/sha256_test.cpp
    unit/simulation_test.cpp
    unit/uuid_test.cpp
    unit/version_test.cpp
)

target_include_directories(engine_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
    ${CMAKE_SOURCE_DIR}/tools/engine/src
)

target_link_libraries(engine_core_tests PRIVATE
    engine::core
    engine::platform
    engine::command
    engine::scene
    Catch2::Catch2WithMain
    spdlog::spdlog
)

engine_set_warnings(engine_core_tests)

add_test(NAME engine_core_tests COMMAND engine_core_tests)
set_tests_properties(engine_core_tests PROPERTIES LABELS "unit")

add_executable(engine_dependencies_smoke
    unit/dependencies_smoke.cpp
)

target_link_libraries(engine_dependencies_smoke PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CLI11::CLI11
    tl::expected
    EnTT::EnTT
    nlohmann_json_schema_validator::validator
    SDL3::SDL3
    glm::glm
    fastgltf::fastgltf
    imgui::imgui
    GPUOpen::VulkanMemoryAllocator
    volk::volk_headers
    volk::volk
)

target_compile_definitions(engine_dependencies_smoke PRIVATE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

engine_set_warnings(engine_dependencies_smoke)

add_test(NAME engine_dependencies_smoke COMMAND engine_dependencies_smoke)
set_tests_properties(engine_dependencies_smoke PROPERTIES LABELS "unit;dependencies")

add_executable(engine_asset_tests
    unit/asset_tangents_test.cpp
    unit/asset_commands_test.cpp
    unit/asset_import_test.cpp
    unit/asset_library_test.cpp
    unit/image_decode_test.cpp
)

target_include_directories(engine_asset_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
)

target_link_libraries(engine_asset_tests PRIVATE
    engine::asset
    engine::scene
    Catch2::Catch2WithMain
    nlohmann_json_schema_validator::validator
)

target_compile_definitions(engine_asset_tests PRIVATE
    ENGINE_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/m3"
    ENGINE_SCHEMA_DIR="${CMAKE_SOURCE_DIR}/schemas"
)

engine_set_warnings(engine_asset_tests)

add_test(NAME engine_asset_tests COMMAND engine_asset_tests)
set_tests_properties(engine_asset_tests PROPERTIES LABELS "unit;asset")

add_executable(engine_render_tests
    render/camera_test.cpp
    render/descriptor_requirements_test.cpp
    render/device_test.cpp
    render/scene_transform_test.cpp
)

target_link_libraries(engine_render_tests PRIVATE
    engine::render
    Catch2::Catch2WithMain
)

engine_set_warnings(engine_render_tests)

add_test(NAME engine_render_tests COMMAND engine_render_tests)
set_tests_properties(engine_render_tests PROPERTIES
    LABELS "unit;render"
    SKIP_RETURN_CODE 4
)

add_executable(engine_screenshot_test
    render/screenshot_test.cpp
)

target_link_libraries(engine_screenshot_test PRIVATE
    engine::render
)

target_include_directories(engine_screenshot_test PRIVATE
    ${Stb_INCLUDE_DIR}
)

engine_set_warnings(engine_screenshot_test)

add_executable(engine_window_smoke
    render/window_smoke.cpp
)

target_link_libraries(engine_window_smoke PRIVATE
    engine::platform
    engine::render
)

engine_set_warnings(engine_window_smoke)

add_executable(engine_swapchain_test
    render/swapchain_test.cpp
)

target_link_libraries(engine_swapchain_test PRIVATE
    engine::platform
    engine::render
)

engine_set_warnings(engine_swapchain_test)

add_executable(engine_overlay_test
    render/overlay_test.cpp
)

target_link_libraries(engine_overlay_test PRIVATE
    engine::platform
    engine::render
)

engine_set_warnings(engine_overlay_test)

# The window, swapchain and overlay tests need a display server. They report
# exit code 77 when no display is available, which CTest renders as skipped.
# Windows presentation remains unverified, so they are not registered there.
if(NOT WIN32)
    add_test(NAME engine_window_smoke COMMAND engine_window_smoke)
    set_tests_properties(engine_window_smoke PROPERTIES
        LABELS "unit;render"
        SKIP_RETURN_CODE 77
    )

    add_test(NAME engine_swapchain_test COMMAND engine_swapchain_test)
    set_tests_properties(engine_swapchain_test PROPERTIES
        LABELS "unit;render"
        SKIP_RETURN_CODE 77
    )

    add_test(NAME engine_overlay_test COMMAND engine_overlay_test)
    set_tests_properties(engine_overlay_test PROPERTIES
        LABELS "unit;render"
        SKIP_RETURN_CODE 77
    )

    add_test(
        NAME engine_run_smoke
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/smoke/run.py"
                "$<TARGET_FILE:engine>"
    )
    set_tests_properties(engine_run_smoke PROPERTIES
        LABELS "contract;cli;render"
        SKIP_RETURN_CODE 77
    )

    add_test(
        NAME engine_point_light_warning
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/smoke/point_light_warning.py"
                "$<TARGET_FILE:engine>"
    )
    set_tests_properties(engine_point_light_warning PROPERTIES
        LABELS "contract;cli;render"
        SKIP_RETURN_CODE 77
    )
endif()

# Offscreen tests are registered on every platform. They skip when no usable
# Vulkan device or software ICD is available.
add_test(NAME engine_screenshot_golden
    COMMAND engine_screenshot_test
            "${CMAKE_SOURCE_DIR}/tests/fixtures/golden/m2_scene_64.png"
)
set_tests_properties(engine_screenshot_golden PROPERTIES
    LABELS "unit;render;acceptance"
    SKIP_RETURN_CODE 77
)

add_test(
    NAME engine_screenshot_cli
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/smoke/screenshot.py"
            "$<TARGET_FILE:engine>"
)
set_tests_properties(engine_screenshot_cli PROPERTIES
    LABELS "contract;cli;render"
    SKIP_RETURN_CODE 77
)

add_test(
    NAME engine_version_json
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/smoke/version_json.py"
            "$<TARGET_FILE:engine>"
)

set_tests_properties(engine_version_json PROPERTIES
    LABELS "unit;cli"
)

add_test(
    NAME engine_cli_contract
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/contract/validate_cli.py"
            "$<TARGET_FILE:engine>"
)

set_tests_properties(engine_cli_contract PROPERTIES
    LABELS "contract;cli"
)

add_test(
    NAME engine_docs_links
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/check_links.py"
)

set_tests_properties(engine_docs_links PROPERTIES
    LABELS "docs"
)

add_test(
    NAME engine_sensitive_scan
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/check_sensitive.py"
)

set_tests_properties(engine_sensitive_scan PROPERTIES
    LABELS "docs;security"
)

add_test(
    NAME engine_acceptance_m1
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/acceptance_m1.py"
            "$<TARGET_FILE:engine>"
)

set_tests_properties(engine_acceptance_m1 PROPERTIES
    LABELS "contract;acceptance"
)

add_test(
    NAME engine_api_reference
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/generate_api_reference.py"
            --engine "$<TARGET_FILE:engine>"
            --check
)

set_tests_properties(engine_api_reference PROPERTIES
    LABELS "docs;contract"
)

add_library(engine_fuzz_harness STATIC
    fuzz/gltf_import_fuzz.cpp
    fuzz/image_decode_fuzz.cpp
)
target_link_libraries(engine_fuzz_harness PRIVATE engine::asset)
engine_set_warnings(engine_fuzz_harness)

if(ENGINE_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ENGINE_BUILD_FUZZERS requires Clang")
    endif()
    add_executable(engine_gltf_fuzz fuzz/gltf_import_fuzz.cpp)
    target_link_libraries(engine_gltf_fuzz PRIVATE engine::asset)
    target_compile_options(engine_gltf_fuzz PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(engine_gltf_fuzz PRIVATE -fsanitize=fuzzer)
    engine_set_warnings(engine_gltf_fuzz)

    add_executable(engine_image_fuzz fuzz/image_decode_fuzz.cpp)
    target_link_libraries(engine_image_fuzz PRIVATE engine::asset)
    target_compile_options(engine_image_fuzz PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(engine_image_fuzz PRIVATE -fsanitize=fuzzer)
    engine_set_warnings(engine_image_fuzz)
endif()

add_test(NAME engine_asset_cli
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/contract/asset_cli.py"
            "$<TARGET_FILE:engine>"
)
set_tests_properties(engine_asset_cli PROPERTIES LABELS "contract;cli;asset")

add_executable(engine_pbr_test render/pbr_test.cpp render/pbr_probes.cpp ${PROJECT_SOURCE_DIR}/tools/engine/src/stb_image_write_impl.cpp)
target_link_libraries(engine_pbr_test PRIVATE engine::render engine::asset)
target_include_directories(engine_pbr_test PRIVATE ${Stb_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_compile_definitions(engine_pbr_test PRIVATE ENGINE_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/m3")
engine_set_warnings(engine_pbr_test)
add_test(NAME engine_pbr_golden COMMAND engine_pbr_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/golden/m3_pbr_256.png")
set_tests_properties(engine_pbr_golden PROPERTIES LABELS "render;asset;acceptance" SKIP_RETURN_CODE 77)
if(NOT WIN32)
    add_test(NAME engine_pbr_window COMMAND engine_pbr_test --window)
    set_tests_properties(engine_pbr_window PROPERTIES LABELS "render;asset" SKIP_RETURN_CODE 77)
endif()
