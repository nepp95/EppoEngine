cmake_minimum_required(VERSION 3.31)

project(EppoEngine LANGUAGES C CXX)

# Compiler flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options("/MP")
endif ()

# Output compile commands with certain generators for debugging purposes
if (CMAKE_GENERATOR STREQUAL "Unix Makefiles" OR CMAKE_GENERATOR MATCHES "^Ninja")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(BUILD_TESTING "Build tests" ON)
option(BUILD_RUNTIME "Build runtime" ON)

# Get dependencies
include(CMake/Dependencies.cmake)

# Targets
add_subdirectory(EppoEngine)
add_subdirectory(EppoEditor)

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(EppoEngineTesting)
endif ()
