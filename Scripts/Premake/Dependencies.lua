local vcpkgTriplet = os.host() == "windows" and "x64-windows" or "x64-linux"
local vcpkgInstalled = EppoRoot .. "/build/vcpkg_installed/" .. os.host() .. "/" .. vcpkgTriplet
local vulkanSdk = os.getenv("VULKAN_SDK") or ""

Dependencies = {
    Dotnet = (os.getenv("DOTNET_ROOT") or "") .. (os.host() == "windows" and "/dotnet.exe" or "/dotnet"),
    IncludeDirectories = {
        Engine = EppoRoot .. "/EppoEngine/Source",
        Vcpkg = vcpkgInstalled .. "/include",
        Tracy = vcpkgInstalled .. "/include/tracy",
        Vulkan = vulkanSdk .. (os.host() == "windows" and "/Include" or "/include"),
    },
    LibraryDirectories = {
        Debug = vcpkgInstalled .. "/debug/lib",
        DebugTracy = vcpkgInstalled .. "/debug/lib/Debug",
        Release = vcpkgInstalled .. "/lib",
        Vulkan = vulkanSdk .. (os.host() == "windows" and "/Lib" or "/lib"),
    },
    RuntimeDirectories = {
        Debug = vcpkgInstalled .. "/debug/bin",
        DebugTracy = vcpkgInstalled .. "/debug/bin/Debug",
        Release = vcpkgInstalled .. "/bin",
    },
    LinuxRuntimeFiles = {
        Debug = {
            vcpkgInstalled .. "/debug/lib/libnvrhi.so",
            vulkanSdk .. "/lib/libdxcompiler.so",
        },
        Release = {
            vcpkgInstalled .. "/lib/libnvrhi.so",
            vulkanSdk .. "/lib/libdxcompiler.so",
        },
    },
    VulkanDxcompiler = vulkanSdk .. "/Bin/dxcompiler.dll",
    WindowsDebugLinks = {
        "box3dd", "efsw", "fmtd", "glfw3dll", "glm", "imguid", "ImGuiFileDialog", "imguizmo",
        "nvrhi", "spdlogd", "spirv-cross-cored", "TracyClient",
        "DirectX-Guids", "DirectX-Headers", "d3d11", "d3d12", "dxcompilerd", "dxgi", "dxguid", "vulkan-1",
        "advapi32", "comdlg32", "gdi32", "ole32", "shell32", "user32", "uuid",
    },
    WindowsReleaseLinks = {
        "box3d", "efsw", "fmt", "glfw3dll", "glm", "imgui", "ImGuiFileDialog", "imguizmo",
        "nvrhi", "spdlog", "spirv-cross-core", "TracyClient",
        "DirectX-Guids", "DirectX-Headers", "d3d11", "d3d12", "dxcompiler", "dxgi", "dxguid", "vulkan-1",
        "advapi32", "comdlg32", "gdi32", "ole32", "shell32", "user32", "uuid",
    },
    LinuxDebugLinks = {
        "box3dd", "efsw", "ImGuiFileDialog", "imguizmo", "imguid", "glfw3",
        "nvrhi", "spdlogd", "fmtd", "TracyClient", "spirv-cross-core", "dxcompiler", "vulkan",
        "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "pthread", "dl", "rt", "m",
    },
    LinuxReleaseLinks = {
        "box3d", "efsw", "ImGuiFileDialog", "imguizmo", "imgui", "glfw3",
        "nvrhi", "spdlog", "fmt", "TracyClient", "spirv-cross-core", "dxcompiler", "vulkan",
        "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "pthread", "dl", "rt", "m",
    },
}
