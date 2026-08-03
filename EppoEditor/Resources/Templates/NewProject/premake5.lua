workspace "$PROJECT_NAME$"
    configurations { "Debug", "Release" }
    platforms "x64"
    startproject "$PROJECT_NAME$.Scripts"

local projectRoot = path.getabsolute(".")
local eppoRoot = path.getabsolute("../../..")
local dotnet = (os.getenv("DOTNET_ROOT") or "") .. (os.host() == "windows" and "/dotnet.exe" or "/dotnet")
local managedProject = projectRoot .. "/Scripts/$PROJECT_NAME$.csproj"
local managedSources = projectRoot .. "/Scripts/Source/**.cs"

if _ACTION ~= "ninja" then
    externalproject "$PROJECT_NAME$.Scripts"
        location "Scripts"
        filename "$PROJECT_NAME$"
        uuid "2A064B7E-294A-4A39-8D80-BD0E68747E21"
        kind "SharedLib"
        language "C#"
else
    project "$PROJECT_NAME$.Scripts"
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
                string.format('touch "%s/Scripts/obj/premake/Debug/lib$PROJECT_NAME$.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Debug/net10.0/$PROJECT_NAME$.dll",
                projectRoot .. "/Scripts/obj/premake/Debug/lib$PROJECT_NAME$.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter { "files:**.csproj", "configurations:Release" }
            buildcommands {
                string.format('"%s" build "%s" -c Release -p:CoreManagedDll="%s" -v m --nologo',
                    dotnet, managedProject, eppoRoot .. "/build/bin/Release-linux-x86_64/EppoScriptCore/EppoScriptCore.dll"),
                string.format('{MKDIR} "%s/Scripts/obj/premake/Release"', projectRoot),
                string.format('touch "%s/Scripts/obj/premake/Release/lib$PROJECT_NAME$.Scripts.a"', projectRoot),
            }
            buildinputs { managedSources }
            buildoutputs {
                projectRoot .. "/Scripts/bin/Release/net10.0/$PROJECT_NAME$.dll",
                projectRoot .. "/Scripts/obj/premake/Release/lib$PROJECT_NAME$.Scripts.a",
            }
            linkbuildoutputs "Off"

        filter {}
end
