vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO jrouwe/JoltPhysics
    REF e77f175595e64cb44218cc9d9d56fc365ad0e36a
    SHA512 f775b0e14682123d24d101eea2110da52d452aae05a61411e2dad2b86004ad665f0740a94e68d813e4b579ee9c7f8707f0cceab99b235d0f25973dac21a2865e
)
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" STATIC_CRT)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/Build"
    OPTIONS
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        -DJPH_BUILD_SHARED_LIBS=OFF -DDOUBLE_PRECISION=ON
        -DCROSS_PLATFORM_DETERMINISTIC=ON -DINTERPROCEDURAL_OPTIMIZATION=OFF
        -DUSE_SSE4_1=ON -DUSE_SSE4_2=ON
        -DUSE_AVX=OFF -DUSE_AVX2=OFF -DUSE_AVX512=OFF -DUSE_F16C=OFF -DUSE_FMADD=OFF
        -DUSE_LZCNT=OFF -DUSE_TZCNT=OFF
        -DJPH_USE_DX12=OFF -DJPH_USE_VK=OFF -DJPH_USE_MTL=OFF -DJPH_USE_CPU_COMPUTE=OFF
        -DUSE_STATIC_MSVC_RUNTIME_LIBRARY=${STATIC_CRT}
        -DOVERRIDE_CXX_FLAGS=OFF -DFLOATING_POINT_EXCEPTIONS_ENABLED=OFF
        -DCPP_RTTI_ENABLED=ON -DCPP_EXCEPTIONS_ENABLED=ON
        -DDEBUG_RENDERER_IN_DEBUG_AND_RELEASE=OFF -DDEBUG_RENDERER_IN_DISTRIBUTION=OFF
        -DPROFILER_IN_DEBUG_AND_RELEASE=OFF -DPROFILER_IN_DISTRIBUTION=OFF
        -DTARGET_UNIT_TESTS=OFF -DTARGET_HELLO_WORLD=OFF -DTARGET_PERFORMANCE_TEST=OFF
        -DTARGET_SAMPLES=OFF -DTARGET_VIEWER=OFF
        -DENABLE_ALL_WARNINGS=OFF
    OPTIONS_RELEASE -DGENERATE_DEBUG_SYMBOLS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME Jolt CONFIG_PATH lib/cmake/Jolt)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Provenance travels with the binary cache, including the exact archive digest.
foreach(config IN ITEMS dbg rel)
    set(build "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${config}")
    set(provenance "${CURRENT_PACKAGES_DIR}/share/${PORT}/build-provenance/${config}")
    file(GLOB compiler_info "${build}/CMakeFiles/*/CMakeCXXCompiler.cmake")
    list(LENGTH compiler_info compiler_count)
    if(NOT compiler_count EQUAL 1)
        message(FATAL_ERROR "Expected one generated C++ compiler identity for ${PORT}/${config}")
    endif()
    file(MAKE_DIRECTORY "${provenance}")
    file(COPY "${build}/compile_commands.json" DESTINATION "${provenance}")
    configure_file("${compiler_info}" "${provenance}/compiler.txt" COPYONLY)
    if(config STREQUAL "dbg")
        set(library_dir "debug/lib")
    else()
        set(library_dir "lib")
    endif()
    file(GLOB archives "${CURRENT_PACKAGES_DIR}/${library_dir}/*.a" "${CURRENT_PACKAGES_DIR}/${library_dir}/*.lib")
    list(LENGTH archives archive_count)
    if(NOT archive_count EQUAL 1)
        message(FATAL_ERROR "Expected one installed ${PORT}/${config} archive")
    endif()
    list(GET archives 0 archive)
    file(SHA256 "${archive}" digest)
    get_filename_component(archive_name "${archive}" NAME)
    file(WRITE "${provenance}/archive-sha256.txt" "${digest} ${library_dir}/${archive_name}\n")
endforeach()
