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
        "%{IncludeDir.bullet}",
		"%{IncludeDir.entt}",
        "%{IncludeDir.glad}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glm}",
		"%{IncludeDir.imgui}",
        "%{IncludeDir.mono}",
        "%{IncludeDir.spdlog}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.tracy}",
        "%{IncludeDir.vulkan}",
		"%{IncludeDir.yaml_cpp}"
    }

    filter "system:windows"
        systemversion "latest"

        defines {
            "EPPO_PLATFORM_WINDOWS"
        }

        links {
			"%{StaticLibrary.glad}",
            "%{StaticLibrary.glfw}",
            "%{StaticLibrary.imgui}",
            "%{StaticLibrary.yaml_cpp}",
			"%{StaticLibrary.winsock}",
			"%{StaticLibrary.winmm}",
			"%{StaticLibrary.winversion}",
			"%{StaticLibrary.bcrypt}",
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
			--"TRACY_ENABLE",
		}

    filter {"system:windows", "configurations:Debug"}

        links {
			"%{StaticLibrary.assimp_debug}",
            "%{StaticLibrary.bullet_common_debug}",
            "%{StaticLibrary.bullet_collision_debug}",
            "%{StaticLibrary.bullet_dynamics_debug}",
            "%{StaticLibrary.bullet_inversedynamics_debug}",
            "%{StaticLibrary.bullet_softbody_debug}",
            "%{StaticLibrary.bullet_linearmath_debug}",
            "%{StaticLibrary.mono_debug}",
            "%{StaticLibrary.shaderc_debug}",
            "%{StaticLibrary.spirv_cross_debug}",
            "%{StaticLibrary.spirv_cross_glsl_debug}"
        }

    filter "configurations:Release"
        defines "EPPO_RELEASE"
        runtime "Release"
        optimize "On"

        defines {
			--"TRACY_ENABLE",
		}

    filter {"system:windows", "configurations:Release"}
        links {
			"%{StaticLibrary.assimp_release}",
            "%{StaticLibrary.bullet_common_release}",
            "%{StaticLibrary.bullet_collision_release}",
            "%{StaticLibrary.bullet_dynamics_release}",
            "%{StaticLibrary.bullet_inversedynamics_release}",
            "%{StaticLibrary.bullet_softbody_release}",
            "%{StaticLibrary.bullet_linearmath_release}",
            "%{StaticLibrary.mono_release}",
            "%{StaticLibrary.shaderc_release}",
            "%{StaticLibrary.spirv_cross_release}",
            "%{StaticLibrary.spirv_cross_glsl_release}"
        }
    
    filter "configurations:Dist"
        defines "EPPO_DIST"
        runtime "Release"
        optimize "On"

    filter {"system:windows", "configurations:Dist"}
        links {
			"%{StaticLibrary.assimp_release}",
            "%{StaticLibrary.bullet_common_release}",
            "%{StaticLibrary.bullet_collision_release}",
            "%{StaticLibrary.bullet_dynamics_release}",
            "%{StaticLibrary.bullet_inversedynamics_release}",
            "%{StaticLibrary.bullet_softbody_release}",
            "%{StaticLibrary.bullet_linearmath_release}",
            "%{StaticLibrary.mono_release}",
            "%{StaticLibrary.shaderc_release}",
            "%{StaticLibrary.spirv_cross_release}",
            "%{StaticLibrary.spirv_cross_glsl_release}"
        }
