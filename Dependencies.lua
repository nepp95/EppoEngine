VulkanSdk = os.getenv("VULKAN_SDK")

if os.getenv("CI") then
    VulkanSdk = "C:/VulkanSDK/1.3.275.0"
end

-- Include directories
IncludeDir = {}
IncludeDir["bullet"] = "%{wks.location}/EppoEngine/Vendor/bullet/include"
IncludeDir["entt"] = "%{wks.location}/EppoEngine/Vendor/entt/single_include"
IncludeDir["filewatch"] = "%{wks.location}/EppoEngine/Vendor/filewatch"
IncludeDir["glfw"] = "%{wks.location}/EppoEngine/Vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/EppoEngine/Vendor/glm"
IncludeDir["googletest"] = "%{wks.location}/EppoEngine/Vendor/googletest/googletest/include"
IncludeDir["imgui"] = "%{wks.location}/EppoEngine/Vendor/imgui"
IncludeDir["mono"] = "%{wks.location}/EppoEngine/Vendor/mono/include"
IncludeDir["spdlog"] = "%{wks.location}/EppoEngine/Vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/EppoEngine/Vendor/stb"
IncludeDir["tinygltf"] = "%{wks.location}/EppoEngine/Vendor/tinygltf"
IncludeDir["tracy"] = "%{wks.location}/EppoEngine/Vendor/tracy/public"
IncludeDir["vma"] = "%{wks.location}/EppoEngine/Vendor/vulkan-memory-allocator"
IncludeDir["vulkan"] = "%{VulkanSdk}/Include"
IncludeDir["yaml_cpp"] = "%{wks.location}/EppoEngine/Vendor/yaml-cpp/include"

-- Static Library directories
StaticLibraryDir = {}
StaticLibraryDir["bullet"] = "%{wks.location}/EppoEngine/Vendor/bullet/lib"
StaticLibraryDir["mono"] = "%{wks.location}/EppoEngine/Vendor/mono/lib"
StaticLibraryDir["vulkan"] = "%{VulkanSdk}/Lib"

-- Static Libraries
StaticLibrary = {}
StaticLibrary["imgui"] = "imgui"
StaticLibrary["yaml_cpp"] = "yaml-cpp"

if (os.target() == "windows") then
    StaticLibrary["bcrypt"] = "Bcrypt.lib"
    StaticLibrary["glfw"] = "glfw"
    StaticLibrary["bullet_common_debug"] = "%{StaticLibraryDir.bullet}/Debug/Bullet3Common_Debug.lib"
    StaticLibrary["bullet_collision_debug"] = "%{StaticLibraryDir.bullet}/Debug/BulletCollision_Debug.lib"
    StaticLibrary["bullet_dynamics_debug"] = "%{StaticLibraryDir.bullet}/Debug/BulletDynamics_Debug.lib"
    StaticLibrary["bullet_inversedynamics_debug"] = "%{StaticLibraryDir.bullet}/Debug/BulletInverseDynamics_Debug.lib"
    StaticLibrary["bullet_softbody_debug"] = "%{StaticLibraryDir.bullet}/Debug/BulletSoftBody_Debug.lib"
    StaticLibrary["bullet_linearmath_debug"] = "%{StaticLibraryDir.bullet}/Debug/LinearMath_Debug.lib"
    StaticLibrary["bullet_common_release"] = "%{StaticLibraryDir.bullet}/Release/Bullet3Common.lib"
    StaticLibrary["bullet_collision_release"] = "%{StaticLibraryDir.bullet}/Release/BulletCollision.lib"
    StaticLibrary["bullet_dynamics_release"] = "%{StaticLibraryDir.bullet}/Release/BulletDynamics.lib"
    StaticLibrary["bullet_inversedynamics_release"] = "%{StaticLibraryDir.bullet}/Release/BulletInverseDynamics.lib"
    StaticLibrary["bullet_softbody_release"] = "%{StaticLibraryDir.bullet}/Release/BulletSoftBody.lib"
    StaticLibrary["bullet_linearmath_release"] = "%{StaticLibraryDir.bullet}/Release/LinearMath.lib"
    StaticLibrary["mono_debug"] = "%{StaticLibraryDir.mono}/Debug/libmono-static-sgen.lib"
    StaticLibrary["mono_release"] = "%{StaticLibraryDir.mono}/Release/libmono-static-sgen.lib"
    StaticLibrary["shaderc_debug"] = "%{StaticLibraryDir.vulkan}/shaderc_sharedd.lib"
    StaticLibrary["shaderc_release"] = "%{StaticLibraryDir.vulkan}/shaderc_shared.lib"
    StaticLibrary["spirv_cross_debug"] = "%{StaticLibraryDir.vulkan}/spirv-cross-cored.lib"
    StaticLibrary["spirv_cross_release"] = "%{StaticLibraryDir.vulkan}/spirv-cross-core.lib"
    StaticLibrary["spirv_cross_glsl_debug"] = "%{StaticLibraryDir.vulkan}/spirv-cross-glsld.lib"
    StaticLibrary["spirv_cross_glsl_release"] = "%{StaticLibraryDir.vulkan}/spirv-cross-glsl.lib"
    StaticLibrary["vulkan"] = "%{StaticLibraryDir.vulkan}/vulkan-1.lib"
    StaticLibrary["winmm"] = "Winmm.lib"
	StaticLibrary["winsock"] = "Ws2_32.lib"
	StaticLibrary["winversion"] = "Version.lib"
else
    StaticLibrary["glfw"] = "glfw3"
    StaticLibrary["shaderc"] = "shaderc_shared"
    StaticLibrary["spirv_cross"] = "spirv-cross-core"
    StaticLibrary["spirv_cross_glsl"] = "spirv-cross-glsl"
end

-- Dynamic Library directories
DynamicLibraryDir = {}
DynamicLibraryDir["mono"] = "%{wks.location}/EppoEngine/Vendor/mono/bin"

-- Dynamic Libraries
DynamicLibrary = {}
DynamicLibrary["mono_debug"] = "%{DynamicLibraryDir.mono}/Debug/mono-2.0-sgen.dll"
DynamicLibrary["mono_release"] = "%{DynamicLibraryDir.mono}/Release/mono-2.0-sgen.dll"
