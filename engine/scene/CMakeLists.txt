add_library(engine_scene STATIC
    src/project.cpp
    src/scene.cpp
)

add_library(engine::scene ALIAS engine_scene)

engine_set_warnings(engine_scene)

target_include_directories(engine_scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_scene PUBLIC cxx_std_20)

target_link_libraries(engine_scene
    PUBLIC
        engine::core
        EnTT::EnTT
    PRIVATE
        engine::platform
)
