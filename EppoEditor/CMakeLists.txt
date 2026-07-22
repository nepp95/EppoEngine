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

# The editor resolves its runtime data (runtimeconfig.json, EppoScriptCore.dll,
# Resources/) relative to the working directory. Visual Studio otherwise launches
# the debugger from the project dir, so point it at the exe output dir — matching
# how CLion/Ninja run it — or scripting fails to load its managed core.
set_target_properties(EppoEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:EppoEditor>")

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

# Copy resources incrementally — only re-copies when files change
set(ENGINE_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/EppoEngine/Resources")
file(GLOB_RECURSE ENGINE_RESOURCE_FILES CONFIGURE_DEPENDS "${ENGINE_RESOURCES_DIR}/*")
file(GLOB_RECURSE EDITOR_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EppoEditor>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_RESOURCES_DIR}" "$<TARGET_FILE_DIR:EppoEditor>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "$<TARGET_FILE_DIR:EppoEditor>/Resources"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync"
    DEPENDS ${ENGINE_RESOURCE_FILES} ${EDITOR_RESOURCE_FILES}
    COMMENT "Copying Resources"
    VERBATIM
)
add_custom_target(EppoResources ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync")
add_dependencies(EppoEditor EppoResources)
