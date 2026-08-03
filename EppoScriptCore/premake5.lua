if _ACTION ~= "ninja" then
    externalproject "EppoScriptCore"
        uuid "CFAB9E47-8220-4C79-88F8-95A5C5B9A75B"
        kind "SharedLib"
        language "C#"
        configurations { "Debug", "Release" }
        configmap {
            ["Dist"] = "Release",
            ["x64"] = "x64",
        }
    return
end

local managedProject = EppoRoot .. "/EppoScriptCore/EppoScriptCore.csproj"
local managedSources = EppoRoot .. "/EppoScriptCore/Source/**.cs"

files { managedProject, managedSources }

filter { "files:**.csproj", "configurations:Debug" }
    buildmessage "Building EppoScriptCore"
    buildcommands {
        string.format('"%s" build "%s" -c Debug -o "%s" -v m --nologo',
            Dependencies.Dotnet, managedProject, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore"),
    }
    buildinputs { managedSources }
    buildoutputs { EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll" }
    linkbuildoutputs "Off"

filter { "files:**.csproj", "configurations:Release" }
    buildmessage "Building EppoScriptCore"
    buildcommands {
        string.format('"%s" build "%s" -c Release -o "%s" -v m --nologo',
            Dependencies.Dotnet, managedProject, EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore"),
    }
    buildinputs { managedSources }
    buildoutputs { EppoRoot .. "/build/bin/" .. OutputDir .. "/EppoScriptCore/EppoScriptCore.dll" }
    linkbuildoutputs "Off"

filter {}
