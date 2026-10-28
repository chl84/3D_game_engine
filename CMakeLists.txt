cmake_minimum_required(VERSION 3.28)

project(engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Version)
include(CompilerWarnings)
include(Sanitizers)
include(Dependencies)

add_subdirectory(engine)
add_subdirectory(tools/engine)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
