add_library(engine_render STATIC
    src/device.cpp
    src/renderer.cpp
    src/vma.cpp
    src/vulkan_util.cpp
)

add_library(engine::render ALIAS engine_render)

engine_set_warnings(engine_render)

target_include_directories(engine_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
