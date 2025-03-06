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
)

foreach(item IN LISTS SOURCES)
    if(WIN32)
        if(item MATCHES "Source/Platform/Linux/.*\\.cpp$")
            list(REMOVE_ITEM SOURCES ${item})
        endif()
    elseif(UNIX)
        if(item MATCHES "Source/Platform/Windows/.*\\.cpp$")
            list(REMOVE_ITEM SOURCES ${item})
        endif()
    endif()
endforeach()

# Includes
set(INCLUDE_DIRS
    "Source"
    "Vendor/filewatch"
    "Vendor/mono/include"
    "Vendor/stb"
    "Vendor/tinygltf"
    "Vendor/vulkan-memory-allocator"
    "Vendor/volk"
)

# Externals
#include(FindPkgConfig)
#set(ENV{PKG_CONFIG_PATH} "/usr/local/bin/lib/pkgconfig")
#pkg_search_module(mono REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE TRACY_ONLY_LOCALHOST)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${FETCHCONTENT_BASE_DIR}/imgui-src
    ${FETCHCONTENT_BASE_DIR}/tracy-src/public
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    EnTT glfw spirv-cross-core spdlog yaml-cpp
)
target_precompile_headers(${PROJECT_NAME} PRIVATE Source/pch.h)
