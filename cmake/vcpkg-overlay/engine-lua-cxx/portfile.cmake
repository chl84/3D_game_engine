vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_download_distfile(ARCHIVE
    URLS "https://www.lua.org/ftp/lua-5.4.9.tar.gz"
    FILENAME "lua-5.4.9.tar.gz"
    SHA512 b112bd3f0d47043568ef52928853ef692554378a5e368d420b589babf606a82ce05fca781f5401c3565171800b39322141ee500e53e813d1a4ccc5d3a05879b1
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" "${SOURCE_PATH}/CMakeLists.txt" COPYONLY)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME engine_lua_cxx CONFIG_PATH share/engine_lua_cxx)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
# The official readme contains the exact source distribution's MIT notice.
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/doc/readme.html")
