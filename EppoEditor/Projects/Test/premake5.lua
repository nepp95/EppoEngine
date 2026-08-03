workspace "Test"
    configurations { "Debug", "Release" }
    platforms "x64"
    startproject "Test.Scripts"

local projectRoot = path.getabsolute(".")
local eppoRoot = path.getabsolute("../../..")
local dotnet = (os.getenv("DOTNET_ROOT") or "") .. (os.host() == "windows" and "/dotnet.exe" or "/dotnet")
local managedProject = projectRoot .. "/Scripts/Test.csproj"
local managedSources = projectRoot .. "/Scripts/Source/**.cs"

if _ACTION ~= "ninja" then
    externalproject "Test.Scripts"
        location "Scripts"
        filename "Test"
        uuid "11317B97-B9E1-4543-BABB-9C6C7EE17E71"
        kind "SharedLib"
        language "C#"
else
    project "Test.Scripts"
        kind "StaticLib"
        language "C++"
        architecture "x86_64"
        targetdir "Scripts/obj/premake/%{cfg.buildcfg}"
        files { managedProject, managedSources }

        filter { "files:**.csproj", "configurations:Debug" }
            buildcommands {
                string.format('"%s" build "%s" -c Debug -p:CoreManagedDll="%s" -v m --nologo',
                    dotnet, managedProject, eppoRoot .. "/build/bin/Debug-linux-x86_64/EppoScriptCore/EppoScriptCore.dll"),
                string.format('{MKDIR} "%s/Scripts/obj/premake/Debug"', projectRoot),
                string.format('touch "%s/Scripts/obj/premake/Debug/libTest.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Debug/net10.0/Test.dll",
                projectRoot .. "/Scripts/obj/premake/Debug/libTest.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter { "files:**.csproj", "configurations:Release" }
            buildcommands {
                string.format('"%s" build "%s" -c Release -p:CoreManagedDll="%s" -v m --nologo',
                    dotnet, managedProject, eppoRoot .. "/build/bin/Release-linux-x86_64/EppoScriptCore/EppoScriptCore.dll"),
                string.format('{MKDIR} "%s/Scripts/obj/premake/Release"', projectRoot),
                string.format('touch "%s/Scripts/obj/premake/Release/libTest.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Release/net10.0/Test.dll",
                projectRoot .. "/Scripts/obj/premake/Release/libTest.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter {}
end
