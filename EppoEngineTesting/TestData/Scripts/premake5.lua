if _ACTION ~= "ninja" then
    externalproject "EppoTesting.Scripts"
        filename "HarnessScripts"
        uuid "8B46E50D-E822-4AB8-820C-16EB2830806D"
        kind "SharedLib"
        language "C#"
        configurations { "Debug", "Release" }
        configmap {
            ["Dist"] = "Release",
            ["x64"] = "x64",
        }
        dependson "EppoScriptCore"
    return
end

local managedProject = EppoRoot .. "/EppoEngineTesting/TestData/Scripts/HarnessScripts.csproj"
local managedSources = EppoRoot .. "/EppoEngineTesting/TestData/Scripts/Source/**.cs"

files { managedProject, managedSources }

filter { "files:**.csproj", "configurations:Debug" }
    buildmessage "Building test harness user assembly"
    buildcommands {
        string.format('"%s" build "%s" -c Debug -o "%s" -p:CoreManagedDll="%s" -v m --nologo',
            Dependencies.Dotnet, managedProject, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoEngineTesting",
            EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll"),
    }
    buildinputs { managedSources, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll" }
    buildoutputs { EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoEngineTesting/EppoTesting.Scripts.dll" }
    linkbuildoutputs "Off"

filter { "files:**.csproj", "configurations:Release" }
    buildmessage "Building test harness user assembly"
    buildcommands {
        string.format('"%s" build "%s" -c Release -o "%s" -p:CoreManagedDll="%s" -v m --nologo',
            Dependencies.Dotnet, managedProject, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoEngineTesting",
            EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll"),
    }
    buildinputs { managedSources, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll" }
    buildoutputs { EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoEngineTesting/EppoTesting.Scripts.dll" }
    linkbuildoutputs "Off"

filter {}
