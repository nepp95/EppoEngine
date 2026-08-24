EppoRoot = path.getabsolute(".")
dofile("Scripts/Premake/Dependencies.lua")
dofile("Scripts/Premake/Testing.lua")

workspace "EppoEngine"
    configurations { "Debug", "Release", "Dist" }
    platforms "x64"
    startproject "EppoEditor"

    OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    if _ACTION ~= "ninja" then
        group "EppoScriptCore"
        include "EppoScriptCore"
        include "EppoEngineTesting/TestData/Scripts"
        group ""
    end

    include "EppoEngine"
    include "EppoEditor"
    include "EppoRuntime"
    include "EppoEngineTesting"

    WriteCTestFiles()
