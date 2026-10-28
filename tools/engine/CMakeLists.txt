add_executable(engine
    src/main.cpp
)

target_link_libraries(engine PRIVATE engine::core)

set_target_properties(engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
