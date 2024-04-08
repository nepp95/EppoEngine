VulkanSdk = os.getenv("VULKAN_SDK")

-- Include directories
IncludeDir = {}
IncludeDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/include"
IncludeDir["entt"] = "%{wks.location}/EppoEngine/Vendor/entt/single_include"
IncludeDir["glad"] = "%{wks.location}/EppoEngine/Vendor/glad/include"
IncludeDir["glfw"] = "%{wks.location}/EppoEngine/Vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/EppoEngine/Vendor/glm"
IncludeDir["googletest"] = "%{wks.location}/EppoEngine/Vendor/googletest/googletest/include"
IncludeDir["imgui"] = "%{wks.location}/EppoEngine/Vendor/imgui"
IncludeDir["mono"] = "%{wks.location}/EppoEngine/Vendor/mono/include"
IncludeDir["spdlog"] = "%{wks.location}/EppoEngine/Vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/EppoEngine/Vendor/stb"
IncludeDir["tracy"] = "%{wks.location}/EppoEngine/Vendor/tracy/public"
IncludeDir["vulkan"] = "%{VulkanSdk}/Include"
IncludeDir["yaml_cpp"] = "%{wks.location}/EppoEngine/Vendor/yaml-cpp/include"

-- Library directories
LibraryDir = {}
LibraryDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/bin"
LibraryDir["mono_lib"] = "%{wks.location}/EppoEngine/Vendor/mono/lib"
LibraryDir["mono_bin"] = "%{wks.location}/EppoEngine/Vendor/mono/bin"
LibraryDir["vulkan"] = "%{VulkanSdk}/Lib"

-- Libraries
Library = {}

Library["glad"] = "glad"
Library["imgui"] = "imgui"
Library["yaml_cpp"] = "yaml-cpp"

if (os.target() == "windows") then
	Library["bcrypt"] = "Bcrypt.lib"
    Library["glfw"] = "glfw"
    Library["assimp_debug"] = "%{LibraryDir.assimp}/Debug/assimp-vc143-mtd.lib"
    Library["assimp_debug_dll"] = "%{LibraryDir.assimp}/Debug/assimp-vc143-mtd.dll"
    Library["assimp_release"] = "%{LibraryDir.assimp}/Release/assimp-vc143-mt.lib"
    Library["assimp_release_dll"] = "%{LibraryDir.assimp}/Release/assimp-vc143-mt.dll"
    Library["mono_debug"] = "%{LibraryDir.mono_lib}/Debug/libmono-static-sgen.lib"
    Library["mono_debug_dll"] = "%{LibraryDir.mono_bin}/Debug/mono-2.0-sgen.dll"
    Library["mono_release"] = "%{LibraryDir.mono_lib}/Release/libmono-static-sgen.lib"
    Library["mono_release_dll"] = "%{LibraryDir.mono_bin}/Release/mono-2.0-sgen.dll"
    Library["shaderc_debug"] = "%{LibraryDir.vulkan}/shaderc_sharedd.lib"
    Library["shaderc_release"] = "%{LibraryDir.vulkan}/shaderc_shared.lib"
    Library["spirv_cross_debug"] = "%{LibraryDir.vulkan}/spirv-cross-cored.lib"
    Library["spirv_cross_release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
    Library["spirv_cross_glsl_debug"] = "%{LibraryDir.vulkan}/spirv-cross-glsld.lib"
    Library["spirv_cross_glsl_release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"
    Library["spirv_tools_debug"] = "%{LibraryDir.vulkan}/SPIRV-Toolsd.lib"
	Library["winmm"] = "Winmm.lib"
	Library["winsock"] = "Ws2_32.lib"
	Library["winversion"] = "Version.lib"
else
    Library["glfw"] = "glfw3"
    Library["assimp"] = "assimp"
    Library["shaderc"] = "shaderc_shared"
    Library["spirv_cross"] = "spirv-cross-core"
    Library["spirv_cross_glsl"] = "spirv-cross-glsl"
    Library["spirv_tools"] = "SPIRV-Tools"
end
