VulkanSdk = os.getenv("VULKAN_SDK")

-- Include directories
IncludeDir = {}
IncludeDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/include"
IncludeDir["entt"] = "%{wks.location}/EppoEngine/Vendor/entt/single_include"
IncludeDir["glfw"] = "%{wks.location}/EppoEngine/Vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/EppoEngine/Vendor/glm"
IncludeDir["googletest"] = "%{wks.location}/EppoEngine/Vendor/googletest/googletest/include"
IncludeDir["imgui"] = "%{wks.location}/EppoEngine/Vendor/imgui"
IncludeDir["spdlog"] = "%{wks.location}/EppoEngine/Vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/EppoEngine/Vendor/stb"
IncludeDir["tracy"] = "%{wks.location}/EppoEngine/Vendor/tracy/public"
IncludeDir["vma"] = "%{wks.location}/EppoEngine/Vendor/vulkan-memory-allocator"
IncludeDir["vulkan"] = "%{VulkanSdk}/Include"
IncludeDir["yaml_cpp"] = "%{wks.location}/EppoEngine/Vendor/yaml-cpp/include"

-- Library directories
LibraryDir = {}
LibraryDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/bin"
LibraryDir["vulkan"] = "%{VulkanSdk}/Lib"

-- Libraries
Library = {}

Library["imgui"] = "imgui"
Library["yaml_cpp"] = "yaml-cpp"

filter "system:windows"
    Library["glfw"] = "glfw"
    Library["vulkan"] = "%{LibraryDir.vulkan}/vulkan-1.lib"
    Library["assimp_debug"] = "%{LibraryDir.assimp}/Debug/assimp-vc143-mtd.lib"
    Library["assimp_debug_dll"] = "%{LibraryDir.assimp}/Debug/assimp-vc143-mtd.dll"
    Library["assimp_release"] = "%{LibraryDir.assimp}/Release/assimp-vc143-mt.lib"
    Library["assimp_release_dll"] = "%{LibraryDir.assimp}/Release/assimp-vc143-mt.dll"
    Library["shaderc_debug"] = "%{LibraryDir.vulkan}/shaderc_sharedd.lib"
    Library["shaderc_release"] = "%{LibraryDir.vulkan}/shaderc_shared.lib"
    Library["spirv_cross_debug"] = "%{LibraryDir.vulkan}/spirv-cross-cored.lib"
    Library["spirv_cross_release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
    Library["spirv_cross_glsl_debug"] = "%{LibraryDir.vulkan}/spirv-cross-glsld.lib"
    Library["spirv_cross_glsl_release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"
    Library["spirv_tools_debug"] = "%{LibraryDir.vulkan}/SPIRV-Toolsd.lib"

filter "system:linux"
    Library["glfw"] = "glfw3"
    Library["vulkan"] = "vulkan"
    Library["assimp"] = "assimp"
    Library["shaderc"] = "shaderc_shared"
    Library["spirv_cross"] = "spirv-cross-core"
    Library["spirv_cross_glsl"] = "spirv-cross-glsl"
    Library["spirv_tools"] = "SPIRV-Tools"