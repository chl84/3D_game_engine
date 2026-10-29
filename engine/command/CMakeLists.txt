add_library(engine_command STATIC
    src/command.cpp
)

add_library(engine::command ALIAS engine_command)

engine_set_warnings(engine_command)

target_include_directories(engine_command
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_command PUBLIC cxx_std_20)

target_link_libraries(engine_command
    PUBLIC
        engine::core
        nlohmann_json::nlohmann_json
    PRIVATE
        nlohmann_json_schema_validator::validator
)
