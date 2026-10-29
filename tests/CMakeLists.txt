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
)

engine_set_warnings(engine_dependencies_smoke)

add_test(NAME engine_dependencies_smoke COMMAND engine_dependencies_smoke)
set_tests_properties(engine_dependencies_smoke PROPERTIES LABELS "unit;dependencies")

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
