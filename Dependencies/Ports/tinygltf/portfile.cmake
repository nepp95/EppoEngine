vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO syoyo/tinygltf
    REF cfcadfa8d14eb489d97b6324838ae100410edcc7
    SHA512 48334f758860a1a7b16e625dcad8b784d3361cf61f8c166f36af57fd33b7823986e0500f0782674733433a92f02fef67be135823d753098a3ea3582612796665
    HEAD_REF master
)

vcpkg_replace_string("${SOURCE_PATH}/tiny_gltf.h" "#include \"json.hpp\"" "#include <nlohmann/json.hpp>")
file(INSTALL
        "${SOURCE_PATH}/tiny_gltf.h"
        "${SOURCE_PATH}/tiny_gltf_v3.h"
        "${SOURCE_PATH}/tinygltf_json.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
