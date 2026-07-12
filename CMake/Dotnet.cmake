# .NET build + runtime provisioning shared by the editor and test targets.
# Included from the root CMakeLists BEFORE the target subdirectories so the
# EppoScriptCore build target and the CopyBuildScripts() helper are available to
# them. Per-target provisioning (POST_BUILD copies, the test harness assembly)
# lives in each target's own CMakeLists, where POST_BUILD steps can attach.
if (${EP_PLATFORM} STREQUAL "EP_PLATFORM_WINDOWS")
    set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet.exe")
elseif (${EP_PLATFORM} STREQUAL "EP_PLATFORM_LINUX")
    if (DEFINED ENV{DOTNET_ROOT})
        set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet")
    else ()
        find_program(DOTNET_EXE dotnet)
    endif ()
endif ()

if (NOT DOTNET_EXE)
    message(FATAL_ERROR "Could not find dotnet!")
endif ()

file(GLOB_RECURSE MANAGED_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/EppoScriptCore/Source/*.cs")

# Build EppoScriptCore.dll
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.dll"
    COMMAND ${DOTNET_EXE} build "${CMAKE_SOURCE_DIR}/EppoScriptCore/EppoScriptCore.csproj" -c $<CONFIG> -o ${CMAKE_CURRENT_BINARY_DIR} -v m --nologo
    COMMENT "Building EppoScriptCore"
    DEPENDS ${MANAGED_SOURCES} "${CMAKE_SOURCE_DIR}/EppoScriptCore/EppoScriptCore.csproj"
    VERBATIM
)
add_custom_target(EppoScriptCore DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.dll")

# Copy EppoScriptCore to build directory
function(CopyBuildScripts target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/EppoScriptCore.dll" $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/EppoScriptCore.pdb" $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/EppoScriptCore.deps.json" $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/EppoEditor/runtimeconfig.json" $<TARGET_FILE_DIR:${target}>
        COMMENT "Copying EppoScriptCore runtime to build directory"
    )
endfunction()
