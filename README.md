# EppoEngine

EppoEngine is a C++20 game engine and editor for Windows and Linux, with Vulkan rendering and .NET 10 scripting.

## Prerequisites

- Python 3.
- Vulkan SDK with `dxc`; set `VULKAN_SDK` to the SDK root.
- .NET SDK 10; set `DOTNET_ROOT` to the SDK root.
- CTest 3.21 or newer. CTest is used directly and does not configure or build Eppo.
- Windows: Visual Studio 2022 or 2026 with the Desktop development with C++ workload.
- Linux: `clang`, `clang++`, `uuid-dev` for building Premake, and the X11/OpenGL development packages required by GLFW. Ubuntu distributes CTest in the `cmake` package, though Eppo does not use CMake to generate or build:

```bash
sudo apt install clang cmake uuid-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config
```

Setup uses Premake 5.0.0-beta8 and vcpkg from the system when available. It checks `VCPKG_ROOT` and then `PATH` for vcpkg, and checks `PATH` for the exact Premake version. Missing Premake or vcpkg can be cloned and built locally under `.eppo/` only after permission is granted. Ninja 1.6 or newer must already be available on Linux. Eppo's installed vcpkg dependency tree is shared by all configurations per host under `build/vcpkg_installed/<System>`; vcpkg keeps its downloads, build trees, packages, and binary cache in its own configured locations.

## Generate build files

On Windows, setup asks whether to generate Visual Studio 2022 or Visual Studio 2026 files. MSVC is the default compiler; pass `--compiler clang` to use Clang instead. Ninja is supported on Linux only.

```bat
Scripts\Setup.bat
```

Linux defaults to Ninja and Clang:

```bash
sh Scripts/setup.sh
```

Setup generates `EppoEngine.sln` for Visual Studio 2022, `EppoEngine.slnx` for Visual Studio 2026, or Linux `build.ninja` at the repository root. The Visual Studio solution groups the real C# projects under EppoScriptCore; those projects expose Debug and Release, with solution Dist mapped to managed Release. Visual Studio project files are generated in their project directories; Premake beta8's Ninja project files sit beside the root `build.ninja`. Generated build files can be opened or built directly; there is no build wrapper.

For Visual Studio, open the generated EppoEngine solution and build the desired project and configuration. Visual Studio builds the real C# projects directly. For Ninja:

```bash
ninja EppoEditor_Debug_x64
ninja EppoEngineTesting_Debug_x64
```

Binaries are written to `build/bin/<Configuration>-<System>-x86_64/<Project>` and intermediates to `build/intermediate/<Configuration>-<System>-x86_64/<Project>`.

Remove all setup and build outputs, including locally provisioned tools, with:

On Windows run `Scripts\Clean.bat`; on Linux run `sh Scripts/clean.sh`.

## Tests

Premake writes standalone CTest manifests alongside each configuration. After building the test target:

```bash
ctest --test-dir build/bin/Debug-linux-x86_64 --output-on-failure
ctest --test-dir build/bin/Debug-linux-x86_64 --label-exclude graphical
```

On Windows, replace `linux` with `windows`. Graphical suites require a display and Vulkan-capable GPU.

## Editor

Run the editor with `EppoEditor/` as its working directory so it reads `Resources/` and `Projects/` from the source tree:

```bash
cd EppoEditor
../build/bin/Debug-linux-x86_64/EppoEditor/EppoEditor "path/to/MyProject.epproj"
```

On Windows the executable has an `.exe` suffix and the output directory contains `windows`. Without an explicit project path, the editor opens `Projects/Test/Test.epproj`.

Every game project contains its own `premake5.lua`. Generating that file creates a project-local solution which references the existing scripts `.csproj`; Visual Studio builds the C# project directly.
