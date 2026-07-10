---
name: core
description: Orientation for the Eppo engine core systems — the application/layer framework, events & input, and the small core systems it carries (ECS scene, entities & components, projects, serialization). Use when working on Application, layers, events, input, the Scene/ECS, components, or projects. NOT rendering (see /rendering), the editor (see /editor), assets, or scripting.
---

# Core sector

The foundational systems in the `EppoEngine/` static lib: the application/layer
framework plus the small core systems it carries (ECS scene, projects). This is
deliberately narrow — **rendering** (`/rendering`) and the **editor** (`/editor`)
are their own sectors, and **assets** and **scripting** are separate subsystems
not covered here.

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
  trailing-return-type style. Also `Core/Log.h`, `UUID.h`, `Buffer.h`, `Hash.h`.

## Scene & ECS — `Source/Scene/`

- EnTT ECS. `Scene.h` (registry owner, play/stop lifecycle), `Entity.h` (thin
  wrapper over `entt::entity` + `Scene*`), `Components.h` (all component structs),
  `SceneSerializer.h` (JSON via nlohmann-json).
- `CameraComponent` holds a `SceneCamera` + `Primary` flag → drives the play
  view. `ScriptComponent` stores only a C# class name — the scripting subsystem
  (separate sector) owns the per-instance field values.

## Projects — `Source/Project/`

- `Project.h` + `ProjectSerializer.h` model a user project (`.epproj`).
- JSON helpers in `Utility/Json.h`; filesystem helpers in `Utility/Filesystem.h`.

## Conventions

- Trailing-return-type style (`auto Foo() -> void`); `Ref<T>` = shared_ptr.
- Prefer logging on error paths over silent returns; keep why-comments to 1–2
  lines.

## Build & test

`EppoEngineTesting/` links `EppoEngine`. Core work is testable **without the
GUI** — prefer a suite over launching the editor. Suites are cost-labeled; the
ones relevant here are `unit` (Core, Input) and `scene` (Scene, Project).

```
cmake --build --preset=windows-debug --target EppoEngineTesting
ctest --test-dir build/debug -C Debug -L unit      # or -L scene
```

`Source/Support/` has the harnesses: `AppHarness` (steppable app loop),
`TestContext`/`ScenarioLayer`, `TempDir`, `GlmCheck`, `SimulatedInput`.
