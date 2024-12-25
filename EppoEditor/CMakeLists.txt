cmake_minimum_required(VERSION 3.24)

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

target_compile_options(EppoEditor PRIVATE ${mono_CFLAGS})
target_link_options(EppoEditor PRIVATE ${mono_LDFLAGS})
target_include_directories(EppoEditor PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoEditor PRIVATE
    EppoEng glfw imgui yaml-cpp
    ${BULLET_COLLISION_LIBRARY} ${BULLET_DYNAMICS_LIBRARY} ${BULLET_MATH_LIBRARY} ${BULLET_SOFTBODY_LIBRARY}
    ${mono_LIBRARIES}
    ${Vulkan_LIBRARY} shaderc_combined spirv-cross-core spirv-cross-glsl
)