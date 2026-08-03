project "EppoRuntime"
    if _ACTION == "ninja" then
        location ".."
    end

    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    architecture "x86_64"
    staticruntime "Off"
    warnings "Default"
    objdir ("%{wks.location}/build/intermediate/" .. OutputDir .. "/%{prj.name}")
    targetdir ("%{wks.location}/build/bin/" .. OutputDir .. "/%{prj.name}")

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    }

    files {
        "Source/**.cpp",
        "Source/**.h",
    }

    includedirs {
        "Source",
        Dependencies.IncludeDirectories.Engine,
        Dependencies.IncludeDirectories.Vcpkg,
        Dependencies.IncludeDirectories.Tracy,
        Dependencies.IncludeDirectories.Vulkan,
    }

    links "EppoEngine"
    debugdir "%{cfg.targetdir}"

    filter "configurations:Debug"
        defines { "EP_DEBUG", "TRACY_ENABLE" }
        runtime "Debug"
        symbols "On"
        libdirs { Dependencies.LibraryDirectories.Debug, Dependencies.LibraryDirectories.DebugTracy, Dependencies.LibraryDirectories.Vulkan }

    filter "configurations:Release"
        defines { "EP_RELEASE", "TRACY_ENABLE" }
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
        kind "WindowedApp"
        defines { "EP_PLATFORM_WINDOWS" }
        systemversion "latest"
        buildoptions "/utf-8"

    filter { "action:vs*", "system:windows", "toolset:msc" }
        multiprocessorcompile "On"

    filter { "action:vs*", "system:windows" }
        buildoptions "/FS"
        linkoptions "/ignore:4099"

    filter { "system:windows", "configurations:Debug" }
        links(Dependencies.WindowsDebugLinks)

    filter { "system:windows", "configurations:Release or Dist" }
        links(Dependencies.WindowsReleaseLinks)

    filter "system:linux"
        defines { "EP_PLATFORM_LINUX", "__EMULATE_UUID" }
        pic "On"
        linkgroups "On"

    filter { "system:linux", "configurations:Debug" }
        links(Dependencies.LinuxDebugLinks)
        runpathdirs "%{cfg.targetdir}"
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.LinuxRuntimeFiles.Debug[1], EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.LinuxRuntimeFiles.Debug[2], EppoRoot, OutputDir),
        }

    filter { "system:linux", "configurations:Release or Dist" }
        links(Dependencies.LinuxReleaseLinks)
        runpathdirs "%{cfg.targetdir}"
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.LinuxRuntimeFiles.Release[1], EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.LinuxRuntimeFiles.Release[2], EppoRoot, OutputDir),
        }

    filter "configurations:Debug or Release"
        postbuildcommands {
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.dll" "%s/build/bin/%s/EppoRuntime"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.pdb" "%s/build/bin/%s/EppoRuntime"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.deps.json" "%s/build/bin/%s/EppoRuntime"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/EppoEditor/runtimeconfig.json" "%s/build/bin/%s/EppoRuntime"', EppoRoot, EppoRoot, OutputDir),
        }

    filter { "action:ninja", "configurations:Dist" }
        prebuildcommands {
            string.format('"%s" build "%s/EppoScriptCore/EppoScriptCore.csproj" -c Release -o "%s/build/bin/Release-%s-x86_64/EppoScriptCore" -v m --nologo',
                Dependencies.Dotnet, EppoRoot, EppoRoot, os.host()),
        }

    filter "configurations:Dist"
        postbuildcommands {
            string.format('{COPYFILE} "%s/build/bin/Release-%s-x86_64/EppoScriptCore/EppoScriptCore.dll" "%s/build/bin/%s/EppoRuntime"',
                EppoRoot, os.host(), EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/Release-%s-x86_64/EppoScriptCore/EppoScriptCore.deps.json" "%s/build/bin/%s/EppoRuntime"',
                EppoRoot, os.host(), EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/EppoEditor/runtimeconfig.json" "%s/build/bin/%s/EppoRuntime"', EppoRoot, EppoRoot, OutputDir),
        }

    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.VulkanDxcompiler, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.RuntimeDirectories.Debug, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.RuntimeDirectories.DebugTracy, EppoRoot, OutputDir),
        }

    filter { "system:windows", "configurations:Release or Dist" }
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.VulkanDxcompiler, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoRuntime"', Dependencies.RuntimeDirectories.Release, EppoRoot, OutputDir),
        }

    filter {}
