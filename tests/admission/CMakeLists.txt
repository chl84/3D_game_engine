cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(engine_m4_admission LANGUAGES CXX)
    include(CTest)
    include(../../cmake/CompilerWarnings.cmake)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()
find_package(engine_lua_cxx CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)

add_executable(engine_m4_admission main.cpp lua_probe.cpp)
target_link_libraries(engine_m4_admission PRIVATE
    engine_lua_cxx::lua sol2::sol2 nlohmann_json::nlohmann_json)
target_compile_definitions(engine_m4_admission PRIVATE
    SOL_ALL_SAFETIES_ON=1 SOL_USING_CXX_LUA=1)
add_executable(engine_m4_jolt_admission main.cpp jolt_probe.cpp)
target_link_libraries(engine_m4_jolt_admission PRIVATE Jolt::Jolt nlohmann_json::nlohmann_json)
target_compile_definitions(engine_m4_jolt_admission PRIVATE M4_JOLT_PROBE_ONLY=1)
# Admission of our bridge always uses warnings-as-errors, including local builds.
foreach(target IN ITEMS engine_m4_admission engine_m4_jolt_admission)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        M4_PROBE_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        M4_PROBE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
    engine_set_warnings(${target})
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Werror -ffp-contract=off)
    endif()
endforeach()
add_test(NAME engine_m4_lua_admission COMMAND engine_m4_admission --verify-lua)
add_test(NAME engine_m4_jolt_admission COMMAND engine_m4_jolt_admission --verify-jolt)
set_tests_properties(engine_m4_lua_admission engine_m4_jolt_admission PROPERTIES
    LABELS "admission;m4" TIMEOUT 30)
