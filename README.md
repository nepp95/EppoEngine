# EppoEngine

[![CI](https://github.com/nepp95/EppoEngine/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/nepp95/EppoEngine/actions/workflows/ci.yml)

EppoEngine is a C++20 game engine and editor I am building as a hobby, mostly to learn more about various related subjects.

## Prerequisites

- Python 3.
- Vulkan SDK with `dxc` (VULKAN_SDK environment variable MUST be set!)
- .NET SDK 10 (DOTNET_ROOT environment variable MUST be set!)
- Windows: Visual Studio 2022 or 2026 with the Desktop development with C++ workload.
- Linux: `clang`, `clang++`, `uuid-dev` for building Premake, and the X11/OpenGL development packages required by GLFW.

```bash
sudo apt install clang cmake uuid-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config
```

## Generate build files

On Windows, setup asks whether to generate Visual Studio 2022 or Visual Studio 2026 files. MSVC is the default compiler; pass `--compiler clang` to use Clang instead. Ninja is supported on Linux only.

Premake is our build system. If it is not detected (or an incompatible version) on the system, the setup will download and build it after permission is given.
Vcpkg is used for dependency management. If it is not detected on the system, the setup will download and build it after permission is given.

```bat
Scripts\Setup.bat
Scripts\GenerateBuildFiles.bat
```

Linux defaults to Ninja and Clang:

```bash
sh Scripts/setup.sh
sh Scripts/generatebuildfiles.sh
```

Setup generates `EppoEngine.sln` for Visual Studio 2022, `EppoEngine.slnx` for Visual Studio 2026, or Linux `build.ninja` at the repository root.

## Cleanup build files

Remove all setup and build outputs, including locally provisioned tools, with:
- Windows: `Scripts\Clean.bat`
- Linux: `sh Scripts/clean.sh`
