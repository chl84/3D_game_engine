vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

if ("docking-experimental" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO ocornut/imgui
        # Exact commit for the annotated v1.93.0-docking tag (the tag object is
        # 4e02e84cbb85d48881388e7090c2dfa1708c02a1). The SHA512 pins the
        # archive bytes, so a moved tag or rewritten commit fails the build.
        REF b916186e66aa3c02b72c6ca17455516512ed8049
        SHA512 eb166f3262a6268a931584863243426f14250f155e063ccf42f6a52f23c3fa65b12089ff5c953d8425aa1d9ef6540721492fff690a21c88082f931fa45ea02e3
        HEAD_REF docking
    )
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO ocornut/imgui
        # Exact commit for the lightweight v1.93.0 tag. The SHA512 pins the
        # commit archive; the docking branch above is pinned separately.
        REF c1aaac65e896a45f81f47a4d68f9959e0f86c859
        SHA512 ce82cd0fba95984e82484f25e107f3a2ace4742624855a32a756b0fc15f668dab7e435db60d651baa55a7f2814839701e423845f83ac755b65349900448b6d8f
        HEAD_REF master
    )
endif()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/imgui-config.cmake.in" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    allegro5-binding            IMGUI_BUILD_ALLEGRO5_BINDING
    android-binding             IMGUI_BUILD_ANDROID_BINDING
    dx9-binding                 IMGUI_BUILD_DX9_BINDING
    dx10-binding                IMGUI_BUILD_DX10_BINDING
    dx11-binding                IMGUI_BUILD_DX11_BINDING
    dx12-binding                IMGUI_BUILD_DX12_BINDING
    glfw-binding                IMGUI_BUILD_GLFW_BINDING
    glut-binding                IMGUI_BUILD_GLUT_BINDING
    metal-binding               IMGUI_BUILD_METAL_BINDING
    opengl2-binding             IMGUI_BUILD_OPENGL2_BINDING
    opengl3-binding             IMGUI_BUILD_OPENGL3_BINDING
    osx-binding                 IMGUI_BUILD_OSX_BINDING
    sdl3-binding                IMGUI_BUILD_SDL3_BINDING
    sdlgpu3-binding             IMGUI_BUILD_SDLGPU3_BINDING
    sdl3-renderer-binding       IMGUI_BUILD_SDL3_RENDERER_BINDING
    vulkan-binding              IMGUI_BUILD_VULKAN_BINDING
    win32-binding               IMGUI_BUILD_WIN32_BINDING
    webgpu-binding              IMGUI_BUILD_WEBGPU_BINDING
    freetype                    IMGUI_FREETYPE
    freetype-svg                IMGUI_FREETYPE_SVG
    wchar32                     IMGUI_USE_WCHAR32
    test-engine                 IMGUI_TEST_ENGINE
)

if ("libigl-imgui" IN_LIST FEATURES)
    vcpkg_download_distfile(
        IMGUI_FONTS_DROID_SANS_H
        URLS
            https://raw.githubusercontent.com/libigl/libigl-imgui/c3efb9b62780f55f9bba34561f79a3087e057fc0/imgui_fonts_droid_sans.h
        FILENAME "imgui_fonts_droid_sans.h"
        SHA512
            abe9250c9a5989e0a3f2285bbcc83696ff8e38c1f5657c358e6fe616ff792d3c6e5ff2fa23c2eeae7d7b307392e0dc798a95d14f6d10f8e9bfbd7768d36d8b31
    )

    file(INSTALL "${IMGUI_FONTS_DROID_SANS_H}" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
endif()

if ("test-engine" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH TEST_ENGINE_SOURCE_PATH
        REPO ocornut/imgui_test_engine
        REF "v${VERSION}"
        SHA512 c5bf34061ca4aaaedeaafd63138c4c6fbfafc2d10b3bcad626c6a54314f2343cbb9d1fc6b77cd6e3e646e265c65cd994235f894ccb4ffc8039a67ef8f05a2265
        HEAD_REF master
    )

    file(REMOVE_RECURSE "${SOURCE_PATH}/test-engine")
    file(COPY "${TEST_ENGINE_SOURCE_PATH}/imgui_test_engine/" DESTINATION "${SOURCE_PATH}/test-engine")
    file(REMOVE_RECURSE "${SOURCE_PATH}/test-engine/thirdparty/stb")
    vcpkg_replace_string("${SOURCE_PATH}/test-engine/imgui_capture_tool.cpp" "//#define IMGUI_STB_IMAGE_WRITE_FILENAME \"my_folder/stb_image_write.h\"" "#define IMGUI_STB_IMAGE_WRITE_FILENAME <stb_image_write.h>\n#define STB_IMAGE_WRITE_STATIC")
    vcpkg_replace_string("${SOURCE_PATH}/imconfig.h" "#pragma once" "#pragma  once\n\n#include \"imgui_te_imconfig.h\"")
    vcpkg_replace_string("${SOURCE_PATH}/test-engine/imgui_te_imconfig.h" "#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 0" "#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
    OPTIONS_DEBUG
        -DIMGUI_SKIP_HEADERS=ON
)

vcpkg_cmake_install()

if ("freetype" IN_LIST FEATURES)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/imconfig.h" "//#define IMGUI_ENABLE_FREETYPE\n" "#define IMGUI_ENABLE_FREETYPE\n")
endif()
if ("freetype-svg" IN_LIST FEATURES)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/imconfig.h" "//#define IMGUI_ENABLE_FREETYPE_PLUTOSVG" "#define IMGUI_ENABLE_FREETYPE_PLUTOSVG")
endif()
if ("wchar32" IN_LIST FEATURES)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/imconfig.h" "//#define IMGUI_USE_WCHAR32" "#define IMGUI_USE_WCHAR32")
endif()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup()

vcpkg_download_distfile(PROGGY_CLEAN_LICENSE
    URLS "https://raw.githubusercontent.com/bluescan/proggyfonts/139ec08a38096161291792313ef5803fc4f0e37b/LICENSE"
    FILENAME "imgui-proggyclean-license-139ec08a38096161291792313ef5803fc4f0e37b.txt"
    SHA512 b17bc797420ceb31de53a3d7e82838ae0b3abbe0f672b0bc3f13e131464843c38649cc911b25661f086b0eee03d5ff3d6e07a075aedcc76cc742a141d9591303
)
vcpkg_download_distfile(PROGGY_FOREVER_LICENSE
    URLS "https://raw.githubusercontent.com/ocornut/proggyforever/f8868ba43bb42c4d88454d03ad81cbd139789f72/LICENSE.txt"
    FILENAME "imgui-proggyforever-license-f8868ba43bb42c4d88454d03ad81cbd139789f72.txt"
    SHA512 dda5f9f404f62dd2d3d3be12ba80db167fb3f9f5b177337fd1133b51d2adb8a58f98eb12adc6f1c7b247299513c7186859c61d4a7e2f4a69135d06b8fc2518d9
)

if ("test-engine" IN_LIST FEATURES)
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt" "${PROGGY_CLEAN_LICENSE}" "${PROGGY_FOREVER_LICENSE}" "${SOURCE_PATH}/test-engine/LICENSE.txt")
else()
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt" "${PROGGY_CLEAN_LICENSE}" "${PROGGY_FOREVER_LICENSE}")
endif()
