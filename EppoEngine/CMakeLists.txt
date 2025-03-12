cmake_minimum_required(VERSION 3.24)

project(EppoEng VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
file(GLOB_RECURSE SOURCES
    "Source/*.cpp"
    "Vendor/stb/stb_image.cpp"
    "Vendor/tinygltf/tinygltf.cpp"
    "Vendor/volk/volk.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_draw.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_internal.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_tables.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_widgets.cpp"
    "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_demo.cpp"
)

# Includes
set(INCLUDE_DIRS
    "Source"
    "Vendor/mono/include"
    "Vendor/stb"
    "Vendor/tinygltf"
    "Vendor/vulkan-memory-allocator"
    "Vendor/volk"
)

# Externals
add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_directories(${PROJECT_NAME} PUBLIC "$ENV{VULKAN_SDK}/Lib")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE TRACY_ONLY_LOCALHOST)
    target_link_directories(${PROJECT_NAME} PUBLIC "Vendor/mono/lib/Debug")
    target_link_libraries(${PROJECT_NAME} PUBLIC spirv-cross-cored)
else()
    target_link_directories(${PROJECT_NAME} PUBLIC "Vendor/mono/lib/Release")
    target_link_libraries(${PROJECT_NAME} PUBLIC spirv-cross-core)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE VK_NO_PROTOTYPES)
target_precompile_headers(${PROJECT_NAME} PRIVATE Source/pch.h)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${FETCHCONTENT_BASE_DIR}/glm-src
    ${FETCHCONTENT_BASE_DIR}/imgui-src
    ${FETCHCONTENT_BASE_DIR}/tracy-src/public
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    # Dependencies
    efsw-static EnTT glfw Jolt libmono-static-sgen spdlog yaml-cpp
    Vulkan::Vulkan Vulkan::shaderc_combined

    # Dependencies of mono
    Bcrypt
    Winmm
    Ws2_32
    Version
)
