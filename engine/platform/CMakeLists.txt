add_library(engine_platform STATIC
    src/atomic_file.cpp
    src/window.cpp
    src/local_socket.cpp
    src/process.cpp
)

add_library(engine::platform ALIAS engine_platform)

engine_set_warnings(engine_platform)

target_include_directories(engine_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_platform PUBLIC cxx_std_20)

target_compile_definitions(engine_platform PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(engine_platform
    PUBLIC
        engine::core
        volk::volk_headers
    PRIVATE
        SDL3::SDL3
)

if(WIN32)
    target_compile_definitions(engine_platform PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(engine_platform PRIVATE ws2_32 advapi32 bcrypt)
endif()
