project "EppoEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
	editandcontinue "Off" -- Necessary for tracy profiler

    targetdir ("%{wks.location}/Bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/Bin-Int/" .. OutputDir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "Source/pch.cpp"

	defines {
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}

    files {
        "Source/**.h",
        "Source/**.cpp",

		"%{IncludeDir.stb}/*.h",
		"%{IncludeDir.stb}/*.cpp",
    }

    includedirs {
        "Source",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.entt}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glm}",
		"%{IncludeDir.imgui}",
        "%{IncludeDir.spdlog}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.tracy}",
        "%{IncludeDir.vma}",
        "%{IncludeDir.vulkan}",
		"%{IncludeDir.yaml_cpp}"
    }

    filter "system:windows"
        systemversion "latest"

        defines {
            "EPPO_PLATFORM_WINDOWS"
        }

        links {
            "%{Library.glfw}",
            "%{Library.imgui}",
            "%{Library.yaml_cpp}",
            "%{Library.vulkan}"
        }

        removefiles {
            "Source/Platform/Linux/**.cpp"
        }
    
    filter "system:linux"
        defines {
            "EPPO_PLATFORM_LINUX"
        }

        removefiles {
            "Source/Platform/Windows/**.cpp"
        }
    
    filter { "system:linux", "action:gmake2" }
        -- fpermissive is specified because of struct members having the same name as the type
        buildoptions {
            "-fpermissive"
        }
    
    filter "configurations:Debug"
        defines "EPPO_DEBUG"
        runtime "Debug"
        symbols "On"

		defines {
			"TRACY_ENABLE",
			"EPPO_TRACK_MEMORY"
		}

    filter {"system:windows", "configurations:Debug"}

        links {
			"%{Library.assimp_debug}",
            "%{Library.shaderc_debug}",
            "%{Library.spirv_cross_debug}",
            "%{Library.spirv_cross_glsl_debug}"
        }

    filter "configurations:Release"
        defines "EPPO_RELEASE"
        runtime "Release"
        optimize "On"

        defines {
			"TRACY_ENABLE",
			"EPPO_TRACK_MEMORY"
		}

    filter {"system:windows", "configurations:Debug"}
        links {
			"%{Library.assimp_release}",
            "%{Library.shaderc_release}",
            "%{Library.spirv_cross_release}",
            "%{Library.spirv_cross_glsl_release}"
        }
    
    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

    filter {"system:windows", "configurations:Debug"}
        links {
			"%{Library.assimp_release}",
            "%{Library.shaderc_release}",
            "%{Library.spirv_cross_release}",
            "%{Library.spirv_cross_glsl_release}"
        }
