vcpkg_from_gitlab(
    OUT_SOURCE_PATH SOURCE_PATH
    GITLAB_URL https://gitlab.com
    REPO nepp95/epposcriptcore
    REF 59b3ce351fc354fd1a9cd5d1015ca873a176dce5
    SHA512 793828a6ed04cbe24a67c07b2b6615cbfbcd4aa5fa789a7f73ede50b16890fcd0d7ed6683cdb7ecfdb33fbde41f59b961ca91330e45c3d9a8991d4d4410a9eac
    HEAD_REF master
    PATCHES
        install-targets.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DBUILD_EXAMPLE=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

include(CMakePackageConfigHelpers)

set(MANAGED_SOURCE_DIR "share/epposcriptcore/EppoScriptCore.Managed")
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/epposcriptcoreConfig.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/epposcriptcore/EppoScriptCoreConfig.cmake"
    INSTALL_DESTINATION "share/epposcriptcore"
    PATH_VARS MANAGED_SOURCE_DIR
)

vcpkg_cmake_config_fixup(CONFIG_PATH "share/epposcriptcore")

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/README.md")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
