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

    links {
        "glfw",
		"imgui",
		"yaml-cpp",
        "%{Library.vulkan}"
    }

    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "EPPO_DEBUG"
        runtime "Debug"
        symbols "On"

		defines {
			"TRACY_ENABLE",
			"EPPO_TRACK_MEMORY"
		}

        links {
            "%{Library.shaderc_debug}",
            "%{Library.spirv_cross_debug}",
            "%{Library.spirv_cross_glsl_debug}"
        }

    filter "configurations:Release"
        defines "EPPO_RELEASE"
        runtime "Release"
        optimize "On"

        links {
            "%{Library.shaderc_release}",
            "%{Library.spirv_cross_release}",
            "%{Library.spirv_cross_glsl_release}"
        }

		defines {
			"TRACY_ENABLE",
			"EPPO_TRACK_MEMORY"
		}
    
    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

        links {
            "%{Library.shaderc_release}",
            "%{Library.spirv_cross_release}",
            "%{Library.spirv_cross_glsl_release}"
        }
