if (${EP_PLATFORM} STREQUAL "EP_PLATFORM_WINDOWS")
    set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet.exe")
elseif (${EP_PLATFORM} STREQUAL "EP_PLATFORM_LINUX")
    if (DEFINED ENV{DOTNET_ROOT})
        set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet")
    else ()
        find_program(DOTNET_EXE dotnet)
    endif ()
endif ()

if (DOTNET_EXE)
    file(GLOB_RECURSE MANAGED_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/EppoScriptCore/Source/*.cs")
    file(GLOB_RECURSE MANAGED_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/EppoScriptCore.Testing/Source/*.cs")

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.dll"
        COMMAND ${DOTNET_EXE} build "${CMAKE_SOURCE_DIR}/EppoScriptCore/EppoScriptCore.csproj" -c $<CONFIG> -o ${CMAKE_CURRENT_BINARY_DIR} -v m --nologo
        COMMENT "Building EppoScriptCore"
        DEPENDS ${MANAGED_SOURCES} "${CMAKE_SOURCE_DIR}/EppoScriptCore/EppoScriptCore.csproj"
        VERBATIM
    )

    add_custom_target(EppoScriptCore ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.dll")
    add_dependencies(EppoEngine EppoScriptCore)
else ()
    message(FATAL_ERROR "Could not find dotnet!")
endif ()