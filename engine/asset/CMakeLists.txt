add_library(engine_asset STATIC
    src/asset_library.cpp
    src/gltf_import.cpp
    src/image_decode.cpp
)

add_library(engine::asset ALIAS engine_asset)

engine_set_warnings(engine_asset)

target_include_directories(engine_asset
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Stb_INCLUDE_DIR}
)

target_compile_features(engine_asset PUBLIC cxx_std_20)

target_link_libraries(engine_asset
    PUBLIC
        engine::core
    PRIVATE
        engine::platform
        fastgltf::fastgltf
)
