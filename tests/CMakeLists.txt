if(EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
    set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
    set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(engine_core_tests
    unit/command_test.cpp
    unit/config_test.cpp
    unit/error_test.cpp
    unit/log_test.cpp
    unit/path_sandbox_test.cpp
    unit/project_test.cpp
    unit/reflect_test.cpp
    unit/scene_test.cpp
    unit/scene_view_test.cpp
    unit/simulation_test.cpp
    unit/uuid_test.cpp
    unit/version_test.cpp
)

target_include_directories(engine_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
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

add_executable(engine_render_tests
    render/camera_test.cpp
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

# The window and swapchain tests need a display server. They report exit code 77
# when no display is available, which CTest renders as skipped. Windows graphics
# is not verified, so the tests are not registered there.
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
endif()

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
