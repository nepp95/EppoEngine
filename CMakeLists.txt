cmake_minimum_required(VERSION 3.14)
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
add_subdirectory(EppoEngine/Vendor/bullet)
add_subdirectory(EppoEngine/Vendor/glfw)
add_subdirectory(EppoEngine/Vendor/imgui)
add_subdirectory(EppoEngine/Vendor/yaml-cpp)

add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)
#add_subdirectory(EppoScripting)
#add_subdirectory(EppoTesting)