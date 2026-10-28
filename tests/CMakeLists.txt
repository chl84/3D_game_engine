find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(engine_core_tests
    unit/version_test.cpp
)

target_link_libraries(engine_core_tests PRIVATE
    engine::core
    Catch2::Catch2WithMain
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
