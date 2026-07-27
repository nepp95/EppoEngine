project(Runtime)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
set(INCLUDE_DIRS
    "Source"
    "${CMAKE_SOURCE_DIR}/EppoEngine/Source"
)

add_executable(EppoRuntime ${SOURCES})
add_dependencies(EppoRuntime EppoScriptCore)
target_include_directories(EppoRuntime PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoRuntime PRIVATE EppoEngine)

set_target_properties(EppoRuntime PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:EppoRuntime>"
    WIN32_EXECUTABLE "$<PLATFORM_ID:Windows>"
)

CopyBuildScripts(EppoRuntime)

if (MSVC)
    target_compile_options(EppoRuntime PRIVATE "/W3")
    target_link_options(EppoRuntime PRIVATE "/ignore:4099")
endif ()

if (WIN32)
    add_custom_command(TARGET EppoRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EppoRuntime> $<TARGET_RUNTIME_DLLS:EppoRuntime>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET EppoRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$ENV{VULKAN_SDK}/Bin/dxcompiler.dll" $<TARGET_FILE_DIR:EppoRuntime>
    )
endif ()

