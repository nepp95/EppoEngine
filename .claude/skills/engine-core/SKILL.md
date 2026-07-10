---
name: engine-core
description: Orientation for the Eppo engine core (non-rendering) — the application/layer framework, ECS scene, entities & components, assets, projects, serialization, and C# scripting. Use when working on Application, layers, events, input, the Scene/ECS, components, scripting, assets, or JSON serialization.
---

# Engine core sector

`EppoEngine/` is the static library. This skill covers everything in it **except
the renderer** (see `/rendering`): the app framework, ECS scene, assets,
projects, and scripting.

## Application framework — `Source/Core/`, `Source/Event/`

- `main()` lives in `Core/EntryPoint.h`; the client implements
  `Eppo::CreateApplication(argc, argv)` (the editor does this once).
- `Core/Application.h` — singleton (`Application::Get()`), owns the `Window`
  (GLFW), `DeviceManager`, layer stack (`PushLayer<T>()`), `ImGuiLayer`. The loop
  is **steppable**: `StepFrame(timestep)` is one frame; `Run()` loops it with a
  wall-clock step. Tests drive `StepFrame` deterministically via `AppHarness`.
- `Core/Layer.h` — `OnAttach/OnDetach/OnUpdate/OnUIRender/OnEvent`. Events
  (`Event/`) dispatch down the layer stack via `OnEvent`.
- `Core/Input.h` routes through an injectable `InputBackend` seam — GLFW in
  production, `Core/SimulatedInput.h` in tests.
- `Core/Base.h` conventions: `Ref<T>`/`CreateRef` (shared_ptr),
  `ScopedPtr<T>`/`CreateScopedPtr` (unique_ptr), `EP_ASSERT(cond, msg)`, config
  macros `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`, Tracy profiling (`EP_PROFILE_FN`),
  trailing-return-type style. `Core/Log.h`, `UUID.h`, `Buffer.h`, `Hash.h`.

## Scene & ECS — `Source/Scene/`

- EnTT ECS. `Scene.h` (registry owner, play/stop lifecycle), `Entity.h` (thin
  wrapper over `entt::entity` + `Scene*`), `Components.h` (all component structs),
  `SceneSerializer.h` (JSON via nlohmann-json).
- `CameraComponent` holds a `SceneCamera` + `Primary` flag → drives the play
  view. `ScriptComponent` stores only a C# class name; per-instance field values
  live in a **side table owned by `ScriptEngine`**, not on the component.

## Assets & projects — `Source/Asset/`, `Source/Project/`

- `Asset/`: `AssetManager` + `AssetImporter`, UUID-keyed metadata
  (`AssetMetadata.h`, `AssetType.h`). glTF meshes via tinygltf.
- `Project/`: `Project.h` + `ProjectSerializer.h` model a user project (`.epproj`).
- JSON helpers in `Utility/Json.h`; filesystem helpers in `Utility/Filesystem.h`.

## Scripting — `Source/Scripting/`

`ScriptEngine.h` is a self-owning singleton (see the class comment): `Init()`
creates it, `Shutdown()` destroys it, everything else is an instance method via
`Get()` — **guard with `IsInitialized()`** at sites that can run before a project
loads. It wraps `EppoScriptCore.Native` (overlay port) which hosts .NET.

- `LoadUserAssembly` loads user C# assemblies; class metadata via
  `GetClasses`/`FindClassIndex`.
- Per-entity lifecycle the scene drives on play: `OnCreateEntity` /
  `OnUpdateEntity` / `OnDestroyEntity`. Live instances registered by entity UUID
  (`GetEntityInstance`).
- **Field storage is a side table** (`ScriptFieldMap`, keyed by entity UUID),
  serialized with the scene and pushed into the live instance on create. Editing
  a field *during play* mutates the live instance and bypasses the side table, so
  stopping restores editor-time values. Fields keyed by name so they survive
  recompilation/reordering.
- `ScriptGlue.h` registers engine callbacks (logging, input) into managed code.

## Build & test

`EppoEngineTesting/` links `EppoEngine`. Suites labeled by cost: `unit` (Core,
Input), `scene` (Scene, Project), `scripting` (needs hosted .NET), `graphical`
(boot the real `Application`, need GPU+display).

```
cmake --build --preset=windows-debug --target EppoEngineTesting
ctest --test-dir build/debug -C Debug           # or: ctest -L unit
```

Core/scene/scripting changes are testable **without the GUI** — prefer a suite
over launching the editor. `Source/Support/` has the harnesses: `AppHarness`,
`TestContext`/`ScenarioLayer`, `TempDir`, `GlmCheck`, `SimulatedInput`.
