configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/engine/core/build_info.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/engine/core/build_info.hpp"
    @ONLY
)

add_library(engine_core STATIC
    src/config.cpp
    src/error.cpp
    src/log.cpp
    src/path_sandbox.cpp
    src/uuid.cpp
    src/version.cpp
)

add_library(engine::core ALIAS engine_core)

engine_set_warnings(engine_core)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_core PUBLIC cxx_std_20)

target_link_libraries(engine_core
    PUBLIC
        nlohmann_json::nlohmann_json
        tl::expected
    PRIVATE
        spdlog::spdlog
)

set_target_properties(engine_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
