project(Editor)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
set(INCLUDE_DIRS
    "Source"
    "${CMAKE_SOURCE_DIR}/EppoEngine/Source"
)

add_executable(EppoEditor ${SOURCES})

target_include_directories(EppoEditor PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoEditor PRIVATE EppoEngine)

if (MSVC)
    target_compile_options(EppoEditor PRIVATE "/W3")
    # LNK4099: spirv-cross-core ships without a matching PDB; harmless.
    target_link_options(EppoEditor PRIVATE "/ignore:4099")
endif ()

# Copy runtime DLLs
add_custom_command(TARGET EppoEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EppoEditor> $<TARGET_RUNTIME_DLLS:EppoEditor>
    COMMAND_EXPAND_LISTS
)

# DXC for runtime shader compilation. The engine links the Vulkan SDK's
# dxcompiler import lib, but the matching DLL isn't a CMake-known runtime
# dependency (it's an imported import-lib target), so TARGET_RUNTIME_DLLS won't
# copy it. Windows also ships a Microsoft dxcompiler.dll (Windows SDK) with no
# SPIR-V backend; if that one is found first on the DLL search path, runtime
# compilation dies with "SPIR-V CodeGen not available". Copy the SDK's DLL next
# to the exe so the exe-directory search (which wins) resolves the SPIR-V build.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(TARGET EppoEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$ENV{VULKAN_SDK}/Bin/dxcompiler.dll" $<TARGET_FILE_DIR:EppoEditor>
        COMMENT "Copying Vulkan DXC (dxcompiler.dll)"
    )
endif ()

# Copy resources incrementally — only re-copies when files change
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EppoEditor>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "$<TARGET_FILE_DIR:EppoEditor>/Resources"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync"
    DEPENDS ${RESOURCE_FILES}
    COMMENT "Copying Resources"
    VERBATIM
)
add_custom_target(EppoResources ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_ResourcesSync")
add_dependencies(EppoEditor EppoResources)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet.exe")

    file(GLOB_RECURSE MANAGED_SOURCES CONFIGURE_DEPENDS "${EppoScriptCore_MANAGED_SOURCE_DIR}/Source/*.cs")

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll"
        COMMAND ${DOTNET_EXE} build "${EppoScriptCore_MANAGED_SOURCE_DIR}/EppoScriptCore.Managed.csproj" -c $<CONFIG> -o ${CMAKE_CURRENT_BINARY_DIR} -v m --nologo
        COMMENT "Building EppoScriptCore.Managed"
        DEPENDS ${MANAGED_SOURCES} "${EppoScriptCore_MANAGED_SOURCE_DIR}/EppoScriptCore.Managed.csproj"
        VERBATIM
    )

    add_custom_target(EppoScriptCoreManaged ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll")
    add_dependencies(EppoEditor EppoScriptCoreManaged)

    add_custom_command(TARGET EppoEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/runtimeconfig.json" $<TARGET_FILE_DIR:EppoEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll" $<TARGET_FILE_DIR:EppoEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.pdb" $<TARGET_FILE_DIR:EppoEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.deps.json" $<TARGET_FILE_DIR:EppoEditor>
        COMMENT "Copying EppoScriptCore runtime"
    )
endif ()