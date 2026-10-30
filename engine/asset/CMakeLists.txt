foreach(entry IN ITEMS
    "mikktspace.c=de87e74107df766ce68108801262bd8d53899414236b59810509a8fc2a51e288"
    "mikktspace.h=17fc433894f24c73753d548086cc4d8c5c0379f4a6edfb98b5da243e4f0bc3d0"
)
    string(REPLACE "=" ";" parts "${entry}")
    list(GET parts 0 filename)
    list(GET parts 1 expected)
    file(SHA256 "${PROJECT_SOURCE_DIR}/third_party/mikktspace/${filename}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "MikkTSpace source checksum mismatch: ${filename}")
    endif()
endforeach()

file(READ "${PROJECT_SOURCE_DIR}/schemas/asset@1.schema.json" ASSET_SCHEMA)
file(READ "${PROJECT_SOURCE_DIR}/schemas/asset-cooked@1.schema.json" COOKED_SCHEMA)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c"
    "${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.h"
    "${PROJECT_SOURCE_DIR}/schemas/asset@1.schema.json"
    "${PROJECT_SOURCE_DIR}/schemas/asset-cooked@1.schema.json"
)
configure_file(src/schemas.hpp.in generated/asset_schemas.hpp @ONLY)

# Keep the pinned upstream distribution intact. Apply only the documented
# random-rotation and vertex-grid UB corrections to the build copy.
file(READ "${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c" mikk_source)
# Fail explicitly if a future upstream repin changes either patch site.
foreach(needle IN ITEMS
    "const float fIndex = g_iCells * ((fVal-fMin)/(fMax-fMin));"
    "t=(uSeed<<t)|(uSeed>>(32-t));"
)
    string(REPLACE "${needle}" "" stripped "${mikk_source}")
    string(LENGTH "${mikk_source}" original_length)
    string(LENGTH "${stripped}" stripped_length)
    string(LENGTH "${needle}" needle_length)
    math(EXPR matches "(${original_length} - ${stripped_length}) / ${needle_length}")
    if(needle MATCHES "^const")
        set(expected_matches 1)
    else()
        set(expected_matches 2)
    endif()
    if(NOT matches EQUAL expected_matches)
        message(FATAL_ERROR "MikkTSpace build-copy patch no longer matches upstream")
    endif()
endforeach()

# A completely collapsed mesh otherwise computes 0/0 and casts NaN to int.
# Double differences also avoid overflow for finite Float32 coordinates.
string(REPLACE "const float fIndex = g_iCells * ((fVal-fMin)/(fMax-fMin));"
    "const float fIndex = fMax > fMin ? (float)(g_iCells * (((double)fVal-fMin)/((double)fMax-fMin))) : 0.0f;"
    mikk_source "${mikk_source}")
string(REPLACE "t=(uSeed<<t)|(uSeed>>(32-t));"
    "t=(uSeed<<t)|(uSeed>>((32-t)&31));" mikk_source "${mikk_source}")
set(mikk_source "/* Altered build copy: rotate-by-zero and grid-cell UB fixes; see third_party/mikktspace/README.md. */\n${mikk_source}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/mikktspace.c"
    CONTENT "${mikk_source}" @ONLY)

add_library(engine_mikktspace STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/generated/mikktspace.c
)
target_include_directories(engine_mikktspace SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/third_party/mikktspace
)

add_library(engine_asset STATIC
    src/asset_library.cpp
    src/gltf_import.cpp
    src/image_decode.cpp
    src/tangents.cpp
    src/commands.cpp
)

add_library(engine::asset ALIAS engine_asset)

engine_set_warnings(engine_asset)

target_include_directories(engine_asset
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Stb_INCLUDE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_features(engine_asset PUBLIC cxx_std_20)

target_link_libraries(engine_asset
    PUBLIC
        engine::core
        engine::command
    PRIVATE
        engine::platform
        fastgltf::fastgltf
        nlohmann_json_schema_validator::validator
        engine_mikktspace
)
