add_executable(engine
    src/commands.cpp
    src/doctor.cpp
    src/main.cpp
)

target_link_libraries(engine PRIVATE
    engine::core
    engine::platform
    engine::command
    engine::scene
    CLI11::CLI11
    volk::volk
    volk::volk_headers
)

engine_set_warnings(engine)

set_target_properties(engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
