# ImGuiFileDialog has no dllexport annotations, so a shared build produces a DLL with
# no exports and no import library. Force a static library on every triplet.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO aiekick/ImGuiFileDialog
    REF v${VERSION}
    SHA512 8bab17a1d11e8b9a730ff5b02c542c9f96cd71a665377507a61c8fb5d743ac0ff60a4049a5c31df8ead58c600534eb97e6e35ce836a1da93415cd80a746edd5c
    HEAD_REF master
    PATCHES
        compact-path-line.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DIGFD_INSTALL=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ImGuiFileDialog CONFIG_PATH lib/cmake/ImGuiFileDialog)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
