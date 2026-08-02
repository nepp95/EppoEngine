project(Editor)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
set(INCLUDE_DIRS
    "Source"
    "${CMAKE_SOURCE_DIR}/EppoEngine/Source"
)

add_executable(EppoEditor ${SOURCES})
add_dependencies(EppoEditor EppoScriptCore)
target_include_directories(EppoEditor PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoEditor PRIVATE EppoEngine)

# The editor reads Resources/ and Projects/ directly from its source directory.
set_target_properties(EppoEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# EppoScriptCore runtime
CopyBuildScripts(EppoEditor)

if (MSVC)
    target_compile_options(EppoEditor PRIVATE "/W3")
    # LNK4099: spirv-cross-core ships without a matching PDB
    target_link_options(EppoEditor PRIVATE "/ignore:4099")
endif ()

# Copy runtime DLLs
add_custom_command(TARGET EppoEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EppoEditor> $<TARGET_RUNTIME_DLLS:EppoEditor>
    COMMAND_EXPAND_LISTS
)

add_custom_command(TARGET EppoEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$ENV{VULKAN_SDK}/Bin/dxcompiler.dll" $<TARGET_FILE_DIR:EppoEditor>
)
