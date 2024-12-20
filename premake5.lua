include "Dependencies.lua"
include "Helpers.lua"

workspace "EppoEngine"
    architecture "x86_64"
    startproject "EppoEditor"
    editandcontinue "Off"

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
        include "EppoEngine/Vendor/googletest"
        include "EppoEngine/Vendor/glfw"
        include "EppoEngine/Vendor/imgui"
        include "EppoEngine/Vendor/spdlog"
        include "EppoEngine/Vendor/yaml-cpp"
    group ""

    group "Core"
        include "EppoEngine"
        include "EppoScripting"
    group ""

    group "Tools"
        include "EppoEditor"
        include "EppoTesting"
    group ""
