cmake_minimum_required(VERSION 3.24)

project(EppoTesting VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
file(GLOB_RECURSE SOURCES
    "Source/*.cpp"
)

# Includes
set(INCLUDE_DIRS
    "Source"
    "${CMAKE_SOURCE_DIR}/EppoEngine/Source"
)

add_executable(EppoTesting ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE EPPO_TESTING)
target_compile_options(${PROJECT_NAME} PRIVATE ${mono_CFLAGS})
target_link_options(${PROJECT_NAME} PRIVATE ${mono_LDFLAGS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
    EppoEng gmock
)
