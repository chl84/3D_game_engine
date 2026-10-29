find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS ENV VULKAN_SDK
    PATH_SUFFIXES Bin bin
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "glslc was not found. Install the Vulkan SDK and make glslc available "
        "on PATH, or set VULKAN_SDK.")
endif()
message(STATUS "Using glslc: ${GLSLC_EXECUTABLE}")

set(shader_dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(shader_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(shader_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${shader_binary_dir}" "${shader_generated_dir}")

set(triangle_vertex_spv "${shader_binary_dir}/triangle.vert.spv")
set(triangle_fragment_spv "${shader_binary_dir}/triangle.frag.spv")
set(shader_header "${shader_generated_dir}/shaders.hpp")

add_custom_command(
    OUTPUT "${triangle_vertex_spv}"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -O
            -o "${triangle_vertex_spv}" "${shader_dir}/triangle.vert"
    DEPENDS "${shader_dir}/triangle.vert" "${GLSLC_EXECUTABLE}"
    COMMENT "Compiling triangle.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${triangle_fragment_spv}"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -O
            -o "${triangle_fragment_spv}" "${shader_dir}/triangle.frag"
    DEPENDS "${shader_dir}/triangle.frag" "${GLSLC_EXECUTABLE}"
    COMMENT "Compiling triangle.frag"
    VERBATIM
)

add_custom_command(
    OUTPUT "${shader_header}"
    COMMAND "${CMAKE_COMMAND}"
            "-DVERTEX=${triangle_vertex_spv}"
            "-DFRAGMENT=${triangle_fragment_spv}"
            "-DOUTPUT=${shader_header}"
            -P "${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${triangle_vertex_spv}" "${triangle_fragment_spv}"
            "${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    COMMENT "Embedding SPIR-V shaders"
    VERBATIM
)

add_custom_target(engine_render_shaders DEPENDS "${shader_header}")

add_library(engine_render STATIC
    src/camera.cpp
    src/device.cpp
    src/renderer.cpp
    src/vma.cpp
    src/vulkan_util.cpp
    "${shader_header}"
)

add_library(engine::render ALIAS engine_render)

engine_set_warnings(engine_render)

target_include_directories(engine_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${shader_generated_dir}"
)

target_compile_features(engine_render PUBLIC cxx_std_20)

# VMA is header-only. Dynamic function loading is required because volk loads
# Vulkan at runtime and defines VK_NO_PROTOTYPES.
target_compile_definitions(engine_render
    PUBLIC
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

target_link_libraries(engine_render
    PUBLIC
        engine::core
        engine::platform
        engine::scene
        volk::volk_headers
        GPUOpen::VulkanMemoryAllocator
        glm::glm
    PRIVATE
        volk::volk
)
