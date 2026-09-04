project "EppoEngine"
    if _ACTION == "ninja" then
        location ".."
    end

    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    architecture "x86_64"
    staticruntime "Off"
    warnings "Default"

    objdir ("%{wks.location}/build/intermediate/" .. OutputDir .. "/%{prj.name}")
    targetdir ("%{wks.location}/build/bin/" .. OutputDir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "Source/pch.cpp"

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    }

    files {
        "Source/**.cpp",
        "Source/**.h",
        "Vendor/**.cpp",
        "Vendor/**.h",
    }

    includedirs {
        "Source",
        "Vendor",
        Dependencies.IncludeDirectories.Vcpkg,
        Dependencies.IncludeDirectories.Tracy,
        Dependencies.IncludeDirectories.Vulkan,
    }

    filter "configurations:Debug"
        defines { "EP_DEBUG", "TRACY_ENABLE", "TRACY_ON_DEMAND" }
        runtime "Debug"
        symbols "On"
        libdirs { Dependencies.LibraryDirectories.Debug, Dependencies.LibraryDirectories.DebugTracy, Dependencies.LibraryDirectories.Vulkan }

    filter "configurations:Release"
        defines { "EP_RELEASE", "TRACY_ENABLE", "TRACY_ON_DEMAND" }
        runtime "Release"
        symbols "On"
        optimize "Speed"
        libdirs { Dependencies.LibraryDirectories.Release, Dependencies.LibraryDirectories.Vulkan }

    filter "configurations:Dist"
        defines "EP_DIST"
        runtime "Release"
        symbols "Off"
        optimize "Full"
        libdirs { Dependencies.LibraryDirectories.Release, Dependencies.LibraryDirectories.Vulkan }

    filter "system:windows"
        defines { "EP_PLATFORM_WINDOWS" }
        systemversion "latest"
        buildoptions "/utf-8"

    filter { "action:vs*", "system:windows", "toolset:msc" }
        multiprocessorcompile "On"

    filter { "action:vs*", "system:windows" }
        buildoptions "/FS"

    filter "system:linux"
        defines { "EP_PLATFORM_LINUX", "__EMULATE_UUID" }
        pic "On"
        removefiles { "Source/Platform/DX12/**" }

    filter {}

    if _ACTION == "ninja" then
        include "../EppoScriptCore"
    else
        dependson "EppoScriptCore"
    end
