add_library(engine_platform STATIC
    src/atomic_file.cpp
)

add_library(engine::platform ALIAS engine_platform)

engine_set_warnings(engine_platform)

target_include_directories(engine_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_platform PUBLIC cxx_std_20)

target_link_libraries(engine_platform
    PUBLIC
        engine::core
)
