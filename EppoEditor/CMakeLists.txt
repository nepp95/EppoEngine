cmake_minimum_required(VERSION 3.24)

project(EppoEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# Sources
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "Source/*.cpp"
)

# Includes
set(INCLUDE_DIRS
    "Source"
    ${CMAKE_SOURCE_DIR}/EppoEngine/Source
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE ${mono_CFLAGS})
target_link_options(${PROJECT_NAME} PRIVATE ${mono_LDFLAGS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
    EppoEng
    ${mono_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_command(
    TARGET EppoEditor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/EppoScripting.dll" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/EppoScripting.pdb" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Scripts"
)
