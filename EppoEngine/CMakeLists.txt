cmake_minimum_required(VERSION 3.24)

project(EppoEng VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
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
    "Vendor/bullet/include"
    "Vendor/bullet/include/bullet"
    "Vendor/entt/single_include"
    "Vendor/filewatch"
    "Vendor/glfw/include"
    "Vendor/glm"
    "Vendor/imgui"
    "Vendor/mono/include"
    "Vendor/spdlog/include"
    "Vendor/stb"
    "Vendor/tinygltf"
    "Vendor/tracy/public"
    "Vendor/vulkan-memory-allocator"
    "Vendor/volk"
    "Vendor/yaml-cpp/include"
)

# Externals
include(FindPkgConfig)
set(ENV{PKG_CONFIG_PATH} "/usr/local/bin/lib/pkgconfig")
pkg_search_module(mono REQUIRED mono-2)
find_package(Bullet REQUIRED)
find_package(Vulkan REQUIRED)

add_library(EppoEng STATIC ${SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(EppoEng PRIVATE TRACY_ENABLE TRACY_ONLY_LOCALHOST)
endif()

target_compile_definitions(EppoEng PRIVATE VK_NO_PROTOTYPES)
target_include_directories(EppoEng PUBLIC ${INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${BULLET_INCLUDE_DIR})
target_precompile_headers(EppoEng PRIVATE Source/pch.h)
