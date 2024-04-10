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
            "%{StaticLibrary.glad}",
            "%{StaticLibrary.glfw}",
            "%{StaticLibrary.shaderc}",
            "%{StaticLibrary.spirv_cross}",
            "%{StaticLibrary.spirv_cross_glsl}",
            "%{StaticLibrary.spirv_tools}",
            "%{StaticLibrary.imgui}",
            "%{StaticLibrary.yaml_cpp}",
            "%{StaticLibrary.assimp}"
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
			'{COPY} "%{DynamicLibrary.assimp_debug}" "%{cfg.targetdir}"',
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
			'{COPY} "%{DynamicLibrary.assimp_release}" "%{cfg.targetdir}"',
		}

    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

    filter {"system:windows", "configurations:Dist"}
		postbuildcommands {
			'{COPY} "%{DynamicLibrary.assimp_release}" "%{cfg.targetdir}"',
		}
