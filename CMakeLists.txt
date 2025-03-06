cmake_minimum_required(VERSION 3.24)
project(EppoEngine)

include(FetchContent)

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
find_package(Bullet REQUIRED)
if(NOT Bullet_FOUND)
    message(FATAL_ERROR "Bullet not found!")
endif()

find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found!")
endif()

FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(entt)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    FIND_PACKAGE_ARGS NAMES glfw3
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8-docking
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(spirv-cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG vulkan-sdk-1.3.275.0
)
FetchContent_MakeAvailable(spirv-cross)

FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.11.1
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(yaml-cpp)

add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)
add_subdirectory(EppoTesting)