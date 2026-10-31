set(VCPKG_BUILD_TYPE release)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ThePhD/sol2
    REF eba86625b707e3c8c99bbfc4624e51f42dc9e561
    SHA512 a1fbcb4efd9a8b8b97c351e90499644aea72a3db62c258e219a2912853936b76870b51e69d835c14cbf1a20733673ba474e259a0243fec419c411b995cd1511d
)
file(INSTALL "${SOURCE_PATH}/include/sol" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/sol2")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/sol2/sol2Config.cmake" [=[
add_library(sol2::sol2 INTERFACE IMPORTED)
set_target_properties(sol2::sol2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../include")
]=])
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
