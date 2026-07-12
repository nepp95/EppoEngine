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

Presets: `{windows,linux}-{debug,release,dist}` = Debug / RelWithDebInfo / Release; build trees `build/{debug,release,dist}`. Debug editor exe: `build/debug/EppoEditor/Debug/EppoEditor.exe`, with runtime DLLs, `Resources/`, and the managed scripting assembly copied beside it by post-build steps. `.clang-format` at the repo root defines the code style.

## Tests

`EppoEngineTesting/` — UnitTest++ runner linking `EppoEngine` (built when `BUILD_TESTING` is ON, the default). One CTest entry per suite (`eppo_add_suite` in its CMakeLists); the runner takes a suite name as first arg. Run via `ctest --preset=windows-debug` (test presets exist for `{windows,linux}-debug`) or `ctest --test-dir build/debug -C Debug`. Suites are labeled by cost — `unit` (Core, Input), `scene` (Scene, Project), `scripting` (needs hosted .NET), `graphical` (App, Scenario — boot the real `Application`, need GPU + display) — filter with `ctest -L unit` / `--label-exclude graphical`. `Source/Support/` helpers: `AppHarness` (steppable app loop for graphical tests), `TestContext`/`ScenarioLayer` (graphical scenarios), `TempDir`, `GlmCheck`. The test target stands alone: post-build steps copy the editor's `Resources/`, `TestData/` (scenario scene + a test user-script assembly), and build the managed scripting core beside the exe; tests run with the exe dir as working directory.

## CI

`.gitlab-ci.yml` — GitLab CI on shared Linux runners. Toolchain (clang, CMake, vcpkg, Vulkan SDK, .NET) lives in a container image (`.gitlab/ci/Dockerfile`) in the project registry, rebuilt only when the Dockerfile changes. The `build-and-test` job builds only the `EppoEngineTesting` target with the `linux-debug` preset, runs everything except the `graphical` label, and publishes JUnit to the MR Tests tab. Compute budget matters: vcpkg binary cache (keyed on `vcpkg.json` + `CMakePresets.json`) is the main lever — avoid churning those files needlessly.

## Architecture

- **EppoEngine/** — static lib; engine code in `Source/`, which is a public include dir (`#include "Renderer/Renderer.h"`); PCH at `Source/pch.h`.
- **EppoEditor/** — editor exe; `EditorLayer` (owns edit/play scene state, `OnScenePlay`/`OnSceneStop`) + dockable panels in `Source/Panels/` (managed by `PanelManager`): Property, Scene Hierarchy, Content Browser (asset/directory browsing with icons, drag-drop sources). `Resources/` (shaders, fonts, icons, meshes, new-project templates) copied incrementally to output dir via stamp file (`_ResourcesSync`).
- **EppoEngineTesting/** — test runner (see Tests above).
- **EppoRuntime/** — placeholder, not built.

### Application framework (Source/Core/)

`main()` lives in `Core/EntryPoint.h`; the client (editor) includes it once and implements `Eppo::CreateApplication(argc, argv)` returning an `Application` subclass. `Application` is a singleton (`Application::Get()`) owning the `Window` (GLFW), `DeviceManager`, layer stack (`PushLayer<T>()`), and `ImGuiLayer`; its loop is steppable (single-frame stepping used by the test `AppHarness`). Events (`Event/`) are dispatched down the layer stack via `OnEvent`. `Input` routes through an injectable `InputBackend` seam — GLFW in production, `SimulatedInput` in tests.

`Core/Base.h` conventions:
- `Ref<T>` = `std::shared_ptr` (`CreateRef<T>()`); `ScopedPtr<T>` = `std::unique_ptr` (`CreateScopedPtr<T>()`)
- `EP_ASSERT(cond, msg)` — logs + debug-breaks in Debug/Release, compiled out in Dist
- Config macros `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`; platform macros `EP_PLATFORM_WINDOWS`/`EP_PLATFORM_LINUX`
- Tracy profiling (`EP_PROFILE_FN`, `EP_FRAME_MARK`) enabled in Debug/Release; global `new`/`delete` overridden for memory tracking
- Trailing return type style (`auto Foo() -> void`)
- Keep comments concise — explain the non-obvious *why* in a line or two, not a paragraph

### Renderer (Source/Renderer/ + Platform/Vulkan/)

Renderer classes (`Shader`, `Pipeline`, `Framebuffer`, buffers, `Image`, `Mesh`) are written against NVRHI handles; Vulkan device/swapchain/shader-compilation code is isolated in `Platform/Vulkan/`. GLSL shaders (`.vert`/`.frag` in `EppoEditor/Resources/Shaders/`) compile at runtime via DXC with a disk cache (`Resources/Shaders/Cache`), reflected with spirv-cross. `SceneRenderer` does scene-level drawing atop the lower-level `Renderer`.

### Scene & assets

`Scene/` — EnTT ECS (`Scene`, `Entity` wrapper, `Components.h`); scenes/projects serialize to JSON (nlohmann-json, helpers in `Utility/Json.h`). `CameraComponent` (holds a `SceneCamera`, `Primary` flag) drives the play-mode view; `ScriptComponent` names a C# class only — per-instance field values live in a side table owned by `ScriptEngine`. `Project/` models a user project. `Asset/` — `AssetManager`/`AssetImporter` with UUID-keyed metadata (glTF meshes via tinygltf).

### Scripting (Source/Scripting/)

`ScriptEngine` is our authority class for the DotNet integration wrapping `EppoScriptCore`, which hosts .NET — it owns itself between `Init(runtimeConfigPath)` and `Shutdown()`, reached via `Get()` (guard with `IsInitialized()`). `LoadUserAssembly` loads user C# assemblies; engine callbacks (logging, input) are registered into the managed side. Per-entity scripting: the scene drives `OnCreateEntity`/`OnUpdateEntity`/`OnDestroyEntity` on play; `ScriptEngine` owns the live-instance registry (`GetEntityInstance`, UUID-keyed `ScriptInstance`s) and the editor-time field storage (`ScriptFieldMap` side table, serialized with the scene and pushed into instances on create). Editing fields during play mutates the live instance directly and bypasses the side table, so stopping restores editor-time values. On Windows, the editor build also runs `dotnet build` on `EppoScriptCore.Managed.csproj` (C# sources from the port) and copies the DLL + `runtimeconfig.json` beside the editor exe.

## General Workflow
Make use of the following skills: systematic-debugging, verification-before-completion, using-git-worktrees, test-driven-development and writing-skills.
Before you start working, make a thorough plan after investigating both the relevant source and the topic in general. Verify key decisions with me at that moment, taking away any possible doubt.
After we made the plan, go to work using the skills I mentioned above. After you are done and used verification-before-completion, please use the code-reviewer to verify your work from a different perspective. Do not launch the code reviewer at regular intervals, just at the end is fine.