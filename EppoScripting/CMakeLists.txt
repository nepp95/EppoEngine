cmake_minimum_required(VERSION 3.24)

project(EppoScripting VERSION 1.0 LANGUAGES CSharp)

set(CMAKE_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8")

file(GLOB_RECURSE SOURCES
    "Source/*.cs"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
