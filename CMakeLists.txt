cmake_minimum_required(VERSION 3.31)
project(EppoEngine)

if (WIN32)
    add_compile_definitions(EPPO_PLATFORM_WINDOWS VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX)
    add_compile_definitions(EPPO_PLATFORM_LINUX VK_USE_PLATFORM_XLIB_KHR)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(EPPO_DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(EPPO_RELEASE)
endif ()

if (MSVC)
    add_compile_options("/MP")
endif ()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Dependencies
set(Vulkan_DIR $ENV{VULKAN_SDK})
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found!")
endif ()

# Entt
find_package(EnTT CONFIG REQUIRED)
if (NOT EnTT_FOUND)
    #message(FATAL_ERROR "EnTT not found!")
endif ()

# Efsw
find_package(efsw CONFIG REQUIRED)
if (NOT efsw_FOUND)
    message(FATAL_ERROR "efsw not found!")
endif ()

# Glfw
find_package(glfw3 CONFIG REQUIRED)
if (NOT glfw3_FOUND)
    message(FATAL_ERROR "glfw3 not found!")
endif ()

# Glm
find_package(glm CONFIG REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "glm not found!")
endif ()

# Google Test
find_package(gtest CONFIG REQUIRED)
if (NOT gtest_FOUND)
    message(FATAL_ERROR "GTest not found!")
endif ()

# ImGui
#vcpkg_cmake_configure(
#    SOURCE_PATH imgui
#    OPTIONS -D
#)
find_package(imgui CONFIG REQUIRED)
if (NOT imgui_FOUND)
    message(FATAL_ERROR "imgui not found!")
endif ()

# Jolt Physics
find_package(Jolt CONFIG REQUIRED)
if (NOT Jolt_FOUND)
    message(FATAL_ERROR "Jolt not found!")
endif ()

# Json (nlohmann)
find_package(nlohmann_json CONFIG REQUIRED)
if (NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann_json not found!")
endif ()

# Spdlog
find_package(spdlog CONFIG REQUIRED)
if (NOT spdlog_FOUND)
    message(FATAL_ERROR "spdlog not found!")
endif ()

# Tracy Profiler
find_package(Tracy CONFIG REQUIRED)
if (NOT Tracy_FOUND)
    message(FATAL_ERROR "Tracy not found!")
endif ()

# Jolt overrides our MSVC Runtime Library variable
# if (MSVC)
#     set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
# endif ()

add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)
add_subdirectory(EppoScripting)
add_subdirectory(EppoTesting)

set_target_properties(EppoEditor PROPERTIES VS_DOTNET_REFERENCES_COPY_LOCAL OFF)
add_dependencies(EppoEditor EppoScripting)
