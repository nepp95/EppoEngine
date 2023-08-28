VulkanSdk = os.getenv("VULKAN_SDK")

-- Include directories
IncludeDir = {}
IncludeDir["entt"] = "%{wks.location}/EppoEngine/Vendor/entt/single_include"
IncludeDir["glfw"] = "%{wks.location}/EppoEngine/Vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/EppoEngine/Vendor/glm"
IncludeDir["imgui"] = "%{wks.location}/EppoEngine/Vendor/imgui"
IncludeDir["spdlog"] = "%{wks.location}/EppoEngine/Vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/EppoEngine/Vendor/stb"
IncludeDir["tracy"] = "%{wks.location}/EppoEngine/Vendor/tracy/public"
IncludeDir["vma"] = "%{wks.location}/EppoEngine/Vendor/vulkan-memory-allocator"
IncludeDir["vulkan"] = "%{VulkanSdk}/Include"

-- Library directories
LibraryDir = {}
LibraryDir["vulkan"] = "%{VulkanSdk}/Lib"

-- Libraries
Library = {}
Library["shaderc_debug"] = "%{LibraryDir.vulkan}/shaderc_sharedd.lib"
Library["shaderc_release"] = "%{LibraryDir.vulkan}/shaderc_shared.lib"
Library["spirv_cross_debug"] = "%{LibraryDir.vulkan}/spirv-cross-cored.lib"
Library["spirv_cross_release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
Library["spirv_cross_glsl_debug"] = "%{LibraryDir.vulkan}/spirv-cross-glsld.lib"
Library["spirv_cross_glsl_release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"
Library["spirv_tools_debug"] = "%{LibraryDir.vulkan}/SPIRV-Toolsd.lib"
Library["vulkan"] = "%{LibraryDir.vulkan}/vulkan-1.lib"
