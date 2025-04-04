cmake_minimum_required(VERSION 3.24)
project(EppoEngine)

include(FetchContent)

if (WIN32)
    add_compile_definitions(EPPO_PLATFORM_WINDOWS VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX)
    add_compile_definitions(EPPO_PLATFORM_LINUX VK_USE_PLATFORM_XLIB_KHR)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
    add_compile_definitions(EPPO_DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_CONFIGURATION_TYPES STREQUAL "RelWithDebInfo")
    add_compile_definitions(EPPO_RELEASE)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES STREQUAL "Release")
    add_compile_definitions(EPPO_DIST)
endif ()

if (MSVC)
    add_compile_options("/MP")
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Dependencies
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found!")
endif ()

# Entt
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
)
FetchContent_MakeAvailable(entt)

# Efsw
FetchContent_Declare(efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG ad4ac9056402940bd41df38597d6046674b1c4a1
)
FetchContent_MakeAvailable(efsw)

# Glfw
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    FIND_PACKAGE_ARGS NAMES glfw3
)
FetchContent_MakeAvailable(glfw)

# Glm
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
)
FetchContent_MakeAvailable(glm)

# ImGui
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8-docking
)
FetchContent_MakeAvailable(imgui)

# Json (nlohmann)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
)
FetchContent_MakeAvailable(nlohmann_json)

# Jolt Physics
FetchContent_Declare(jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(jolt)

# Spdlog
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# Tracy Profiler
FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.11.1
)
FetchContent_MakeAvailable(tracy)

# Jolt overrides our MSVC Runtime Library variable
if (MSVC)
    set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)
add_subdirectory(EppoScripting)
add_subdirectory(EppoTesting)

add_dependencies(EppoEditor EppoScripting)