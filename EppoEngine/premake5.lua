project "EppoEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir ("%{wks.location}/Bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/Bin-Int/" .. OutputDir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "Source/pch.cpp"

	defines {
		"GLFW_INCLUDE_NONE"
	}

    files {
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs {
        "Source",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.vma}",
        "%{IncludeDir.vulkan}"
    }

    links {
        "glfw",
        "%{Library.vulkan}"
    }

    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "EPPO_DEBUG"
        runtime "Debug"
        symbols "On"

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
    
    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

        links {
            "%{Library.shaderc_release}",
            "%{Library.spirv_cross_release}",
            "%{Library.spirv_cross_glsl_release}"
        }
