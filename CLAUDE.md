# CLAUDE.md

Guidance for Claude Code when working in this repo.

## Overview

Eppo: C++20 game engine — `EppoEngine` (static lib) + `EppoEditor` (ImGui editor exe). Rendering: Vulkan via NVRHI; ECS: EnTT; C# scripting via hosted .NET runtime. `EppoRuntime` is a stub, not in the build.

## Environment

Required env vars (configure fails without them): `VCPKG_ROOT` (deps come from the vcpkg manifest `vcpkg.json`), `VULKAN_SDK`, `DOTNET_ROOT` (builds the managed scripting assembly on Windows). `CMake/Ports/` holds vcpkg overlay ports for `nvrhi` and `epposcriptcore` (scripting glue, https://gitlab.com/nepp95/epposcriptcore).

## Build

CMake presets (clang/clang-cl on both platforms) define everything — compiler, build type, toolchain, build dir — no extra flags:

```
cmake --preset=windows-debug          # configure (linux-debug on Linux)
cmake --build --preset=windows-debug  # build
```

Presets: `{windows,linux}-{debug,release,dist}` = Debug / RelWithDebInfo / Release; build trees `build/{debug,release,dist}`. Debug editor exe: `build/debug/EppoEditor/Debug/EppoEditor.exe`, with runtime DLLs, `Resources/`, and the managed scripting assembly copied beside it by post-build steps. A `BUILD_TESTING` option and `unittest-cpp` vcpkg dep exist, but no test target yet.

## Architecture

- **EppoEngine/** — static lib; engine code in `Source/`, which is a public include dir (`#include "Renderer/Renderer.h"`); PCH at `Source/pch.h`.
- **EppoEditor/** — editor exe; `EditorLayer` + dockable panels in `Source/Panels/` (managed by `PanelManager`). `Resources/` (shaders, fonts, meshes, new-project templates) copied incrementally to output dir via stamp file (`_ResourcesSync`).
- **EppoRuntime/** — placeholder, not built.

### Application framework (Source/Core/)

`main()` lives in `Core/EntryPoint.h`; the client (editor) includes it once and implements `Eppo::CreateApplication(argc, argv)` returning an `Application` subclass. `Application` is a singleton (`Application::Get()`) owning the `Window` (GLFW), `DeviceManager`, layer stack (`PushLayer<T>()`), and `ImGuiLayer`. Events (`Event/`) are dispatched down the layer stack via `OnEvent`.

`Core/Base.h` conventions:
- `Ref<T>` = `std::shared_ptr` (`CreateRef<T>()`); `ScopedPtr<T>` = `std::unique_ptr` (`CreateScopedPtr<T>()`)
- `EP_ASSERT(cond, msg)` — logs + debug-breaks in Debug/Release, compiled out in Dist
- Config macros `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`; platform macros `EP_PLATFORM_WINDOWS`/`EP_PLATFORM_LINUX`
- Tracy profiling (`EP_PROFILE_FN`, `EP_FRAME_MARK`) enabled in Debug/Release; global `new`/`delete` overridden for memory tracking
- Trailing return type style (`auto Foo() -> void`)

### Renderer (Source/Renderer/ + Platform/Vulkan/)

Renderer classes (`Shader`, `Pipeline`, `Framebuffer`, buffers, `Image`, `Mesh`) are written against NVRHI handles; Vulkan device/swapchain/shader-compilation code is isolated in `Platform/Vulkan/`. GLSL shaders (`.vert`/`.frag` in `EppoEditor/Resources/Shaders/`) compile at runtime via DXC with a disk cache (`Resources/Shaders/Cache`), reflected with spirv-cross. `SceneRenderer` does scene-level drawing atop the lower-level `Renderer`.

### Scene & assets

`Scene/` — EnTT ECS (`Scene`, `Entity` wrapper, `Components.h`); scenes/projects serialize to JSON (nlohmann-json, helpers in `Utility/Json.h`). `Project/` models a user project. `Asset/` — `AssetManager`/`AssetImporter` with UUID-keyed metadata (glTF meshes via tinygltf).

### Scripting (Source/Scripting/)

`ScriptEngine` is a static singleton wrapping `EppoScriptCore.Native` (from the overlay port), which hosts .NET. On Windows, the editor build also runs `dotnet build` on `EppoScriptCore.Managed.csproj` (C# sources from the port) and copies the DLL + `runtimeconfig.json` beside the editor exe. `ScriptEngine::Init` takes the runtime config path; `LoadUserAssembly` loads user C# assemblies; engine callbacks (logging, input) are registered into the managed side.

## Changes

Big changes: create a new git branch. Small changes: `develop`. After changes, create a PR for the branch you edited but do NOT merge it — the code reviewer will weigh in.