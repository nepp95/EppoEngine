project "EppoEngineTesting"
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

    links { "EppoEngine", "UnitTest++" }
    if _ACTION ~= "ninja" then
        dependson { "EppoScriptCore", "EppoTesting.Scripts" }
    end
    debugdir "%{wks.location}/EppoEditor"

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
        runpathdirs { Dependencies.LibraryDirectories.Debug, Dependencies.LibraryDirectories.DebugTracy, Dependencies.LibraryDirectories.Vulkan }

    filter { "system:linux", "configurations:Release or Dist" }
        links(Dependencies.LinuxReleaseLinks)
        runpathdirs { Dependencies.LibraryDirectories.Release, Dependencies.LibraryDirectories.Vulkan }

    filter {}

    if _ACTION == "ninja" then
        include "TestData/Scripts"
    end

    filter "configurations:Debug or Release"
        postbuildcommands {
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.dll" "%s/build/bin/%s/EppoEngineTesting"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.pdb" "%s/build/bin/%s/EppoEngineTesting"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/%s/EppoScriptCore/EppoScriptCore.deps.json" "%s/build/bin/%s/EppoEngineTesting"', EppoRoot, OutputDir, EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/EppoEditor/runtimeconfig.json" "%s/build/bin/%s/EppoEngineTesting"', EppoRoot, EppoRoot, OutputDir),
            string.format('{MKDIR} "%s/build/bin/%s/EppoEngineTesting/TestData"', EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s/EppoEngineTesting/TestData/Scenes" "%s/build/bin/%s/EppoEngineTesting/TestData/Scenes"', EppoRoot, EppoRoot, OutputDir),
        }

    filter { "action:ninja", "configurations:Dist" }
        prebuildcommands {
            string.format('"%s" build "%s/EppoScriptCore/EppoScriptCore.csproj" -c Release -o "%s/build/bin/Release-%s-x86_64/EppoScriptCore" -v m --nologo',
                Dependencies.Dotnet, EppoRoot, EppoRoot, os.host()),
            string.format('"%s" build "%s/EppoEngineTesting/TestData/Scripts/HarnessScripts.csproj" -c Release -o "%s/build/bin/Release-%s-x86_64/EppoEngineTesting" -p:CoreManagedDll="%s/build/bin/Release-%s-x86_64/EppoScriptCore/EppoScriptCore.dll" -v m --nologo',
                Dependencies.Dotnet, EppoRoot, EppoRoot, os.host(), EppoRoot, os.host()),
        }

    filter "configurations:Dist"
        postbuildcommands {
            string.format('{COPYFILE} "%s/build/bin/Release-%s-x86_64/EppoScriptCore/EppoScriptCore.dll" "%s/build/bin/%s/EppoEngineTesting"',
                EppoRoot, os.host(), EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/Release-%s-x86_64/EppoScriptCore/EppoScriptCore.deps.json" "%s/build/bin/%s/EppoEngineTesting"',
                EppoRoot, os.host(), EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/build/bin/Release-%s-x86_64/EppoEngineTesting/EppoTesting.Scripts.dll" "%s/build/bin/%s/EppoEngineTesting"',
                EppoRoot, os.host(), EppoRoot, OutputDir),
            string.format('{COPYFILE} "%s/EppoEditor/runtimeconfig.json" "%s/build/bin/%s/EppoEngineTesting"', EppoRoot, EppoRoot, OutputDir),
            string.format('{MKDIR} "%s/build/bin/%s/EppoEngineTesting/TestData"', EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s/EppoEngineTesting/TestData/Scenes" "%s/build/bin/%s/EppoEngineTesting/TestData/Scenes"', EppoRoot, EppoRoot, OutputDir),
        }

    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoEngineTesting"', Dependencies.VulkanDxcompiler, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoEngineTesting"', Dependencies.RuntimeDirectories.Debug, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoEngineTesting"', Dependencies.RuntimeDirectories.DebugTracy, EppoRoot, OutputDir),
        }

    filter { "system:windows", "configurations:Release or Dist" }
        postbuildcommands {
            string.format('{COPYFILE} "%s" "%s/build/bin/%s/EppoEngineTesting"', Dependencies.VulkanDxcompiler, EppoRoot, OutputDir),
            string.format('{COPYDIR} "%s" "%s/build/bin/%s/EppoEngineTesting"', Dependencies.RuntimeDirectories.Release, EppoRoot, OutputDir),
        }

    filter {}
