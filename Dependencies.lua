VulkanSdk = os.getenv("VULKAN_SDK")

-- Include directories
IncludeDir = {}
IncludeDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/include"
IncludeDir["bullet"] = "%{wks.location}/EppoEngine/Vendor/bullet/include"
IncludeDir["entt"] = "%{wks.location}/EppoEngine/Vendor/entt/single_include"
IncludeDir["glad"] = "%{wks.location}/EppoEngine/Vendor/glad/include"
IncludeDir["glfw"] = "%{wks.location}/EppoEngine/Vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/EppoEngine/Vendor/glm"
IncludeDir["googletest"] = "%{wks.location}/EppoEngine/Vendor/googletest/googletest/include"
IncludeDir["imgui"] = "%{wks.location}/EppoEngine/Vendor/imgui"
IncludeDir["spdlog"] = "%{wks.location}/EppoEngine/Vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/EppoEngine/Vendor/stb"
IncludeDir["tracy"] = "%{wks.location}/EppoEngine/Vendor/tracy/public"
IncludeDir["vulkan"] = "%{VulkanSdk}/Include"
IncludeDir["yaml_cpp"] = "%{wks.location}/EppoEngine/Vendor/yaml-cpp/include"

-- Static Library directories
StaticLibraryDir = {}
StaticLibraryDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/lib"
StaticLibraryDir["bullet"] = "%{wks.location}/EppoEngine/Vendor/bullet/lib"
StaticLibraryDir["vulkan"] = "%{VulkanSdk}/Lib"

-- Static Libraries
StaticLibrary = {}
StaticLibrary["glad"] = "glad"
StaticLibrary["imgui"] = "imgui"
StaticLibrary["yaml_cpp"] = "yaml-cpp"

if (os.target() == "windows") then
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
    StaticLibrary["assimp_debug"] = "%{StaticLibraryDir.assimp}/Debug/assimp-vc143-mtd.lib"
    StaticLibrary["assimp_release"] = "%{StaticLibraryDir.assimp}/Release/assimp-vc143-mt.lib"
    StaticLibrary["shaderc_debug"] = "%{StaticLibraryDir.vulkan}/shaderc_sharedd.lib"
    StaticLibrary["shaderc_release"] = "%{StaticLibraryDir.vulkan}/shaderc_shared.lib"
    StaticLibrary["spirv_cross_debug"] = "%{StaticLibraryDir.vulkan}/spirv-cross-cored.lib"
    StaticLibrary["spirv_cross_release"] = "%{StaticLibraryDir.vulkan}/spirv-cross-core.lib"
    StaticLibrary["spirv_cross_glsl_debug"] = "%{StaticLibraryDir.vulkan}/spirv-cross-glsld.lib"
    StaticLibrary["spirv_cross_glsl_release"] = "%{StaticLibraryDir.vulkan}/spirv-cross-glsl.lib"
    StaticLibrary["spirv_tools_debug"] = "%{StaticLibraryDir.vulkan}/SPIRV-Toolsd.lib"
else
    StaticLibrary["glfw"] = "glfw3"
    StaticLibrary["assimp"] = "assimp"
    StaticLibrary["shaderc"] = "shaderc_shared"
    StaticLibrary["spirv_cross"] = "spirv-cross-core"
    StaticLibrary["spirv_cross_glsl"] = "spirv-cross-glsl"
    StaticLibrary["spirv_tools"] = "SPIRV-Tools"
end

-- Dynamic Library directories
DynamicLibraryDir = {}
DynamicLibraryDir["assimp"] = "%{wks.location}/EppoEngine/Vendor/assimp/bin"

-- Dynamic Libraries
DynamicLibrary = {}
DynamicLibrary["assimp_debug"] = "%{DynamicLibraryDir.assimp}/Debug/assimp-vc143-mtd.dll"
DynamicLibrary["assimp_release"] = "%{DynamicLibraryDir.assimp}/Release/assimp-vc143-mt.dll"
