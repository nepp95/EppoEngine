project "EppoEditor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir ("%{wks.location}/Bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/Bin-Int/" .. OutputDir .. "/%{prj.name}")

    files {
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs {
        "Source",
        "%{wks.location}/EppoEngine/Source",
        "%{wks.location}/EppoEngine/Vendor"
    }

    links {
        "EppoEngine"
    }

    filter "system:windows"
		systemversion "latest"

    filter "configurations:Debug"
        defines "EPPO_DEBUG"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines "EPPO_RELEASE"
        runtime "Release"
        optimize "On"
    
    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"