workspace "Chess"
    configurations { "Debug", "Release" }
    platforms "x64"
    startproject "Chess.Scripts"

local projectRoot = path.getabsolute(".")
local eppoRoot = path.getabsolute("../../..")
local dotnet = (os.getenv("DOTNET_ROOT") or "") .. (os.host() == "windows" and "/dotnet.exe" or "/dotnet")
local managedProject = projectRoot .. "/Scripts/Chess.csproj"
local managedSources = projectRoot .. "/Scripts/Source/**.cs"

if _ACTION ~= "ninja" then
    externalproject "Chess.Scripts"
        location "Scripts"
        filename "Chess"
        uuid "5C3E8A21-7F4D-4B9E-A6C2-9D1E4F7B3A08"
        kind "SharedLib"
        language "C#"
else
    project "Chess.Scripts"
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
                string.format('touch "%s/Scripts/obj/premake/Debug/libChess.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Debug/net10.0/Chess.dll",
                projectRoot .. "/Scripts/obj/premake/Debug/libChess.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter { "files:**.csproj", "configurations:Release" }
            buildcommands {
                string.format('"%s" build "%s" -c Release -p:CoreManagedDll="%s" -v m --nologo',
                    dotnet, managedProject, eppoRoot .. "/build/bin/Release-linux-x86_64/EppoScriptCore/EppoScriptCore.dll"),
                string.format('{MKDIR} "%s/Scripts/obj/premake/Release"', projectRoot),
                string.format('touch "%s/Scripts/obj/premake/Release/libChess.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Release/net10.0/Chess.dll",
                projectRoot .. "/Scripts/obj/premake/Release/libChess.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter {}
end
