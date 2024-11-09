EPPO_DIR = "../../../../../"

workspace "Test"
    architecture "x86_64"
    startproject "Test"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

    flags {
        "MultiProcessorCompile"
    }

project "Test"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.8"

    targetdir "Binaries"
    objdir "Intermediates"

    files {
        "Source/**.cs"
    }

    links {
        "EppoScripting"
    }

    filter "configurations:Debug"
        optimize "Off"
        symbols "Default"
    
    filter "configurations:Release"
        optimize "On"
        symbols "Default"
    
    filter "configurations:Release"
        optimize "On"
        symbols "Off"

group "EppoEngine"
    include (EPPO_DIR .. "EppoScripting")