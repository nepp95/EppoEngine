EPPO_DIR = "../../../../../"

workspace "Sandbox"
	architecture "x86_64"
	startproject "Sandbox"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}

	flags {
		"MultiProcessorCompile"
	}

project "Sandbox"
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

	filter "configurations:Dist"
		optimize "On"
		symbols "Off"

group "EppoEngine"
	include (EPPO_DIR .. "EppoScripting")
