project "EppoTesting"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir ("%{wks.location}/Bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/Bin-Int/" .. OutputDir .. "/%{prj.name}")

    defines {
        "EPPO_TESTING",
		"TRACY_NO_INVARIANT_CHECK=1"
    }

	dependson {
		"EppoEngine"
	}

    files {
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs {
        "Source",
        "%{wks.location}/EppoEngine/Source",

		-- TODO: Remove some of the dependencies here
		"%{IncludeDir.entt}",
		"%{IncludeDir.glm}",
        "%{IncludeDir.googletest}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.tracy}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.vma}"
    }

    links {
        "googletest",
        "EppoEngine"
    }

    filter "Configurations:Debug"
        runtime "Debug"
        symbols "On"
    
    filter "Configurations:Release"
        runtime "Release"
        optimize "On"
    
    filter "Configurations:Dist"
        runtime "Release"
        optimize "On"
