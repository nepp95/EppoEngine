include "Dependencies.lua"

workspace "EppoEngine"
    architecture "x86_64"
    startproject "EppoEditor"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

    flags {
        "MultiProcessorCompile"
    }

    OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    group "Dependencies"
        --include "EppoEngine/Vendor/glad"
        --include "EppoEngine/Vendor/glfw"
        --include "EppoEngine/Vendor/imgui"
    group ""

    group "Core"
        include "EppoEngine"
    group ""

    group "Tools"
        include "EppoEditor"
    group ""