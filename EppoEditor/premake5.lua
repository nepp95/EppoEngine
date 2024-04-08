project "EppoEditor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir ("%{wks.location}/Bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/Bin-Int/" .. OutputDir .. "/%{prj.name}")

	dependson {
		"EppoScripting"
	}

    files {
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs {
        "Source",
        "%{wks.location}/EppoEngine/Source",
        "%{wks.location}/EppoEngine/Vendor",

		"%{IncludeDir.entt}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.imgui}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.tracy}"
    }

    links {
        "EppoEngine"
    }

    filter "system:windows"
		systemversion "latest"

        defines {
            "EPPO_PLATFORM_WINDOWS"
        }
    
    filter "system:linux"
        buildoptions {
            "-fpermissive"
        }
    
        defines {
            "EPPO_PLATFORM_LINUX"
        }

        libdirs {
            "/usr/local/lib"
        }

        links {
            "%{Library.glad}",
            "%{Library.glfw}",
            "%{Library.shaderc}",
            "%{Library.spirv_cross}",
            "%{Library.spirv_cross_glsl}",
            "%{Library.spirv_tools}",
            "%{Library.imgui}",
            "%{Library.yaml_cpp}",
            "%{Library.assimp}"
        }

    filter "configurations:Debug"
        defines "EPPO_DEBUG"
        runtime "Debug"
        symbols "On"

		defines {
			--"TRACY_ENABLE"
		}

    filter {"system:windows", "configurations:Debug"}
		postbuildcommands {
			'{COPY} "%{Library.assimp_debug_dll}" "%{cfg.targetdir}"',
            '{COPY} "%{Library.mono_debug_dll}" "%{cfg.targetdir}"'
		}

    filter "configurations:Release"
        defines "EPPO_RELEASE"
        runtime "Release"
        optimize "On"

		defines {
			--"TRACY_ENABLE"
		}

    filter {"system:windows", "configurations:Release"}
		postbuildcommands {
			'{COPY} "%{Library.assimp_release_dll}" "%{cfg.targetdir}"',
            '{COPY} "%{Library.mono_release_dll}" "%{cfg.targetdir}"'
		}

    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

    filter {"system:windows", "configurations:Dist"}
		postbuildcommands {
			'{COPY} "%{Library.assimp_release_dll}" "%{cfg.targetdir}"',
            '{COPY} "%{Library.mono_release_dll}" "%{cfg.targetdir}"'
		}
