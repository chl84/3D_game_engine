add_executable(engine
    src/commands.cpp
    src/doctor.cpp
    src/main.cpp
    src/run.cpp
    src/stb_image_write_impl.cpp
)

target_link_libraries(engine PRIVATE
    engine::core
    engine::platform
    engine::command
    engine::asset
    engine::scene
    engine::render
    CLI11::CLI11
    volk::volk
    volk::volk_headers
    GPUOpen::VulkanMemoryAllocator
    SDL3::SDL3
)

target_include_directories(engine PRIVATE ${Stb_INCLUDE_DIR})

engine_set_warnings(engine)

set_target_properties(engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
