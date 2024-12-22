cmake_minimum_required(VERSION 3.14)

project(EppoEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
file(GLOB_RECURSE SOURCES
    "Source/*.cpp"
)

# Includes
set(INCLUDE_DIRS
    "Source"
    "Vendor/imgui"
    ${CMAKE_SOURCE_DIR}/EppoEngine/Source
)

add_executable(EppoEditor ${SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
endif()

target_include_directories(EppoEditor PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoEditor PRIVATE EppoEng imgui yaml-cpp glfw)