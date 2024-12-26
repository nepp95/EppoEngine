cmake_minimum_required(VERSION 3.24)
project(EppoEngine)

if(WIN32)
    add_compile_definitions(EPPO_PLATFORM_WINDOWS VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    add_compile_definitions(EPPO_PLATFORM_LINUX)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(EPPO_DEBUG)
else()
    add_compile_definitions(EPPO_RELEASE)
endif()

# Dependencies
if(DEFINED ENV{VULKAN_SDK})
    link_directories(AFTER $ENV{VULKAN_SDK}/lib)
else()
    message(FATAL_ERROR "'VULKAN_SDK' not found in environment variables!")
endif()

set(CMAKE_LIBRARY_ARCHIVE_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(EppoEngine/Vendor/bullet)
add_subdirectory(EppoEngine/Vendor/glfw)
add_subdirectory(EppoEngine/Vendor/imgui)
add_subdirectory(EppoEngine/Vendor/yaml-cpp)
add_subdirectory(EppoEngine/Vendor/googletest)

add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)
add_subdirectory(EppoTesting)