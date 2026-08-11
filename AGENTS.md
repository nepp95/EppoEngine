# AGENTS.md

Compact guide for agents working in this repo. Read before editing.

## Project

EppoEngine — a C++20 cross-platform (Windows/Linux) game engine + editor with C# scripting via CoreCLR (.NET 10) and Vulkan rendering through NVRHI. Built with Premake 5.0.0-beta8 + vcpkg (manifest mode).

## Prerequisites (validated by `Scripts/Setup.py`)

- **Vulkan SDK** with `dxc` (`VULKAN_SDK` set).
- **vcpkg** is discovered through `VCPKG_ROOT`, then `PATH`, or provisioned locally with permission. Manifest mode; overlay ports are in `Dependencies/Ports` (imguifiledialog, nvrhi, tinygltf).
- **.NET SDK 10** with `DOTNET_ROOT` set (managed core targets `net10.0`).
- **compiler**: Windows defaults to MSVC and optionally supports Clang; Linux uses `clang`/`clang++`.
- **Ninja 1.6+** on Linux.
- **CTest 3.21+** for running the generated standalone test manifests.
- Linux also needs `uuid-dev` to build Premake and X11/GL dev libs: `libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`. Ubuntu's `cmake` package supplies the standalone CTest executable; Eppo does not use CMake to generate or build.

## Commands

Run `Scripts\Setup.bat` on Windows and choose VS2022 or VS2026. Run `sh Scripts/setup.sh` on Linux for Ninja + Clang. Windows generates one `EppoEngine` solution; its real C# projects are grouped under EppoScriptCore and map solution Dist to managed Release. Generated solutions/build files are used directly; there is no build wrapper. Binary dirs are `build/bin/<Configuration>-<System>-x86_64/<Project>`.
Run `Scripts\GenerateBuildFiles.bat` on Windows or `sh Scripts/generatebuildfiles.sh` on Linux to only rerun Premake (`Setup.py --generate-only`) after the first setup: it reuses the action, compiler, Premake and vcpkg root recorded in `.eppo/build.json`, skips tool provisioning and `vcpkg install`, and never prompts. `--action`/`--compiler` still override.
Run `Scripts\Clean.bat` on Windows or `sh Scripts/clean.sh` on Linux to remove all setup and build outputs, including locally provisioned tools under `.eppo`.

```bash
Scripts\Setup.bat --action vs2026                                  # generate Visual Studio 2026 on Windows
Scripts\GenerateBuildFiles.bat                                     # regenerate only, reusing the recorded setup
# Build EppoEngineTesting in the generated solution
ctest --test-dir build/bin/Debug-windows-x86_64 --output-on-failure
ctest --test-dir build/bin/Debug-windows-x86_64 --label-exclude graphical
ctest --test-dir build/bin/Debug-windows-x86_64 -R Scripting
```

Run a suite directly from `EppoEditor/` so source resources resolve correctly: `../build/bin/Debug-windows-x86_64/EppoEngineTesting/EppoEngineTesting --gtest_filter=Scripting.*` (Google Test; suite = the first `TEST(Suite, Name)` argument). CTest passes exactly this filter per suite.
Suites and labels (registered in `Scripts/Premake/Testing.lua`): `Core`, `Physics`, `Scene` (`core`); `FileDialogFilter`, `Project` (`unit`); `Scripting`, `ScriptMarshalling` (`scripting`); `App`, `CoreGraphical`, `ProjectExport`, `Renderer` (`graphical`).
Visual Studio's built-in Test Adapter for Google Test discovers the suite in Test Explorer with no per-developer setup. The runner's `main.cpp` `chdir`s to `EppoEditor` on startup (via the premake-baked `EP_TEST_WORKING_DIR`), so `Resources/`/`Projects/`/`TestData/` resolve for graphical and data-driven suites regardless of how the exe is launched (Test Explorer runs it from the output dir; CTest also sets `WORKING_DIRECTORY`).

Required order: **generate → build → test**. After editing C# only, rebuild the `EppoEngineTesting` (or `EppoEditor`) target so the dotnet custom commands re-run and DLLs are re-copied.

## Architecture

### Targets

- `EppoEngine/` — static library, the engine. `Source/` modules: `Asset`, `Core`, `Event`, `ImGui`, `Physics`, `Platform`, `Project`, `Renderer`, `Scene`, `Scripting`, `Utility`. Public umbrella header `Source/EppoEngine.h`; PCH `Source/pch.h`. `Core/Buffer/` is the binary serialization substrate: abstract `StreamWriter`/`StreamReader` with `Buffer*` (in-memory) and `FileStream*` (on-disk) implementations, plus paired `StreamSerializable`/`StreamDeserializable` concepts backing `WriteObject`/`ReadObject`. `GameData` is built on it. `Core/ThreadPool/` is the background-task pool owned by `Application` (`GetThreadPool()`): priority/dependency-queued tasks, named task groups with `TaskGroupSnapshot` statistics, and main-thread `Flush`/`CancelAll`/`Shutdown`; the editor's `StatusBar` renders its live state.
- `EppoEditor/` — editor executable (`EppoEditor.cpp` → `EditorLayer`). Depends on `EppoEngine` + `EppoScriptCore`. Owns `Resources/` and `runtimeconfig.json`.
- `EppoScriptCore/` — C# class library (net10.0). Visual Studio exposes the real `.csproj` in the EppoScriptCore solution group and maps solution Dist to managed Release. Ninja invokes `dotnet` through the project Premake definition. Namespaces mirror the folder path minus `Source/`.
- `EppoEngineTesting/` — Google Test runner (custom `main.cpp` wraps `RUN_ALL_TESTS` with logging + `AppHarness::Shutdown`). `Source/` suites mirror engine modules; `Source/TestSupport/` has `AppHarness` (boots a real `Application` for graphical suites), `TestContext` + `ScenarioLayer` (multi-frame scene/camera scenarios, used by the `Renderer` suite), and the `EppoTest.h` / `GlmCheck.h` / `TempDir.h` helpers. `EppoTest.h` provides `EP_REQUIRE`/`EP_REQUIRE_EQ` (a fatal check usable in value-returning helpers where `ASSERT_*` cannot) and `EP_EXPECT_ARRAY_EQ`; `GlmCheck.h` keeps `CHECK_VEC*/MAT4_CLOSE` on `EXPECT_NEAR`. `TestData/Scripts/` builds the `EppoTesting.Scripts.dll` harness the Scripting suite loads. Suites are registered in `Scripts/Premake/Testing.lua`.
- `EppoRuntime/` — standalone player. Reads `Game.eppak` before creating the application, since its engine shaders come from there. It stages **no** `Resources/`: shader sources and their includes travel in the pack, and it never reads them from disk. Logs and its shader cache are written beside the executable.
- `Scripts/Premake/` — shared dependency names and standalone CTest manifest generation. Each native target owns a `premake5.lua`; vcpkg overlays live under `Dependencies/Ports`.

Key libraries: entt (ECS), NVRHI (Vulkan RHI), GLFW + ImGui (docking), glm, box3d (physics), spdlog, tinygltf, Tracy, Google Test.

### Packaging and the deployed runtime (spans Project, Asset, Renderer, EppoRuntime)

- `ProjectExporter::Export` stages the runtime executable and writes `Game.eppak`. `GameData` owns the byte layout — it is documented in full at the top of `Project/GameData.h`, and every section is read/written through the `Core/Buffer/` streams.
- `Asset/PackFormat.h` holds the four-character magics and versions (`EPAK` package, `ESHD` shaders, `EMSH` mesh, `ESCN` scene). Bump the version whenever a layout changes.
- Shaders are packed as **text**, keyed by name, alongside their `#include` sources keyed by path relative to `Resources/Shaders`. A packed `ShaderSpecification` carries both and resolves includes through a handler that never touches the filesystem, so a deployed game cannot silently fall back to a disk compile.
- `AssetManager` has a packed mode (constructed with owned `PackedAssetData` payloads) that loads lazily from memory instead of disk. There is no `PackedAssetManager` class — the test file of that name exercises `AssetManager`.
- `ApplicationParams::PackedShaders` / `PackedShaderIncludes` carry the shader text from the pack into `Renderer::LoadShaders`, which the `Application` constructor calls after `InitRenderer()` and **before** ImGui attaches (`ImGuiRenderer` grabs `GetShader("imgui")` during `ImGuiLayer::OnAttach`). Empty means "compile from `Resources/Shaders`", which is what the editor and tests do.

### C#↔C++ scripting bridge (spans both languages — read as one system)

- `Scripting/RuntimeHost` boots CoreCLR via hostfxr using the `runtimeconfig.json` next to the exe; **CoreCLR initializes once per process** and cannot be re-initialized.
- `ScriptEngine` (singleton, `Init`/`Shutdown`) loads `EppoScriptCore.dll` (core assembly) plus a user assembly, holds per-entity `ScriptInstance`s keyed by entity UUID, and owns the editor-time field side table (`ScriptFieldMap`) — the authoritative, serialized copy of script fields, pushed into the managed instance on create.
- `ScriptGlue.cpp` registers the native functions; on the C# side `EppoScriptCore/Source/Core/InternalCalls.cs` is the **sole unsafe hub** — all `[UnmanagedCallersOnly]`/extern glue lives there, wrapped by friendly APIs (`Entity`, `Components`, `Input`, `Log`, `Physics`).
- Internal-call conventions: structs passed by pointer, entity UUID is the first argument, the live scene is resolved through `ScriptEngine`'s scene context (set on play, cleared on stop/unload). The active `PhysicsWorld` is held weakly so callbacks no-op after scene stop.
- The scene drives per-entity script lifecycle (`OnCreateEntity`/`OnUpdateEntity`/`OnDestroyEntity`).
- **Hot reload:** `ScriptEngine` owns a `FileWatcher` over the project's `Scripts` directory, polled from `VerifyRuntime`. A detected change only sets `m_ReloadPending` and returns — the rebuild happens on a later frame, so a burst of saves collapses into one build, and it is skipped entirely while a scene context is set (i.e. during play). `ReloadProjectAssembly` shells out to `dotnet build` via `Utility/Process`, then unloads and reloads the collectible user assembly. Editor field storage survives; live managed instances do not.

## Gotchas

- **Run the editor from `EppoEditor/`; run tests through CTest.** The editor and graphical tests resolve `Resources/` and `Projects/` from the working directory, while `runtimeconfig.json`, `EppoScriptCore.dll`, and test assemblies resolve beside their executable. CTest sets the source working directory automatically.
- **Managed projects follow the generated build system.** Visual Studio builds the real `.csproj` projects; Ninja invokes `dotnet` custom rules. Post-build steps copy managed outputs beside the native executable.
- **Graphical suites (`App`, `CoreGraphical`, `ProjectExport`, `Renderer`) need a real display + GPU.** They early-return if `AppHarness` can't boot; on headless/CI use `--label-exclude graphical`.
- **"SPIR-V CodeGen not available"** at runtime means the Microsoft `dxcompiler.dll` is shadowing the Vulkan SDK one; copy the Vulkan SDK's `dxcompiler.dll` next to the exe.
- **`EppoRuntime` owns its entry point.** It defines `EP_CUSTOM_ENTRY_POINT` (suppressing the `main` in `Core/EntryPoint.h`) and calls `Eppo::RunApplication` from its own `WinMain`/`main`, so it can wrap startup in a try/catch that reports through `ErrorDialog`. It reads `Game.eppak` inside `CreateApplication` — before the `Application` exists — because the shaders it hands to `ApplicationParams` are needed during construction.
- **Where files get written is configured, not assumed.** `FS::ConfigureWritableDirectory` sets the root that `FS::GetWritableDirectory` and `FS::GetShaderCacheDirectory` resolve against; the runtime points it at its own executable directory so logs and the shader cache land beside the game. Unconfigured, the shader cache falls back to `Resources/Shaders/Cache`.
- **Platform/config macros:** `EP_PLATFORM_WINDOWS`/`EP_PLATFORM_LINUX`; `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`; `TRACY_ENABLE` in Debug and RelWithDebInfo. Linux defines `__EMULATE_UUID`.
- **`UUID::operator bool` is explicit.** Use `static_cast<uint64_t>(uuid)` to get the raw id; implicit numeric conversion is a compile error by design.
- **`RelationshipComponent` is optional.** An entity with no parent or children has no relationship component. Readers guard its absence with `HasComponent`; parenting adds it lazily and unparenting removes it when empty.
- **Scene graph has two walk directions that can disagree.** The hierarchy panel and `Scene::GatherColliders` walk **down** via `Children`; `GetWorldTransform` and the collider wireframe pass walk **up** via `Parent` / iterate the whole registry (`ForEachEntity`). A one-directional link (child names a parent that doesn't list it back, e.g. a scene stored with only the child's `Parent`) is invisible to the down-walkers but still rendered — an entity you can't select/delete whose collider keeps drawing, and whose collider never joins the compound body. `SceneSerializer::Deserialize` must reconcile both directions on load.
- **Launch the editor and capture its startup log to observe runtime state.** This is *not* headless — it spins up the full GUI app (real window, Vulkan swapchain, ImGui, file dialogs); there is no headless editor run (it needs a display + GPU, same as the graphical test harness). From `EppoEditor/`, run `../build/bin/Debug-windows-x86_64/EppoEditor/EppoEditor.exe > out.txt 2>&1 &`, wait a few seconds, `taskkill //IM EppoEditor.exe //F`. It loads the project default scene and logs to `latest.log` + stdout; useful for confirming startup or a fix in the real app rather than trusting tests alone. Describe such runs as "launched the editor and checked its log," never as "headless."
- **Drive the real editor for visual feature verification.** On Windows, launch the built `EppoEditor.exe` with `EppoEditor/` as its working directory, focus its window, and use OS input automation (`user32` cursor/mouse calls plus `SendKeys`) to exercise ImGui. Select the hierarchy entity before coordinate-based property edits; double-click numeric drag fields to enter text. Keep verification edits unsaved, capture the editor window with `GetWindowRect` + `Graphics.CopyFromScreen`, and select a different entity when you need to distinguish persistent scene visualization from ImGuizmo. Stop only the editor process you launched and keep captures outside the repository.

## Style

Conventions below are near-universal in `Core`, `Platform/Vulkan` and `Renderer` — treat a deviation as a mistake, not a choice.

**Formatting**

- `.clang-format`: 4-space indent, 140-col limit, Allman braces, pointer left (`int* p`), `SortIncludes: Never`, namespaces indented.
- **Indentation is 4 spaces.** Remaining tabs in the older engine files are legacy, not a convention, and are pending a one-time repo-wide conversion. Write new and edited code with spaces; don't copy a tab-indented neighbour's whitespace.
- `.clang-tidy`: `bugprone-*`, `clang-diagnostic-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `misc-use-anonymous-namespace`.

**Declarations**

- **Trailing return types, everywhere**: `auto Name(args) -> T`, including `-> void`. This covers members, free functions, lambdas (`[this](Event& e) -> void`), `main`, `WinMain`, and friend declarations. There is not one classic `bool Foo()` declaration in the engine.
- `[[nodiscard]]` on const getters and anything returning a computed value; not on mutating `-> void`. Trivial getters are `constexpr` and defined inline in the header.
- `const` on by-value params in definitions (`auto WriteData(const char* data, const size_t size)`); `const auto` for locals by default.
- Concepts over SFINAE — `StreamSerializable`, `ResourceType`, `requires(std::derived_from<T, Layer>)`.

**Naming and layout**

- `m_` members, `s_` statics and file-scope constants, `g_` globals. PascalCase for methods and public struct fields; camelCase for params and locals. Getters are `Get*` or `Is*`.
- Headers use `#pragma once`, never include guards. Class body order is `public:` → `private:` methods → a **second** `private:` for data members.
- Configurable types take one `XSpecification`/`XParams` struct with PascalCase fields and in-class defaults, constructed at the call site with designated initializers (`WindowSpecification{ .Title = ..., .Width = ... }`).
- File-local helpers go in an anonymous namespace nested inside `namespace Eppo` — never `static` free functions.

**Includes**

- `.cpp` files open with `#include "pch.h"`, then the file's own header, blank line, then project headers (quoted, module-relative from `Source/`), then third-party `<...>`, then std `<...>`. `SortIncludes: Never`, so this order is hand-maintained.
- `pch.h` already supplies the common std headers plus `Core/Base.h`, `Core/Buffer/Buffer.h`, `Core/Hash.h`, `Core/UUID.h`, `Utility/Filesystem.h`, `Utility/Random.h`. Engine headers rely on it (`Renderer/Image.h` names `Buffer` and `std::filesystem::path` with no include of its own) — don't add redundant includes for these.
- Forward-declare only to break include cycles; otherwise `#include`.

**Engine vocabulary** (`Core/Base.h`)

- `Ref<T>`/`CreateRef` (shared), `ScopedPtr<T>`/`CreateScopedPtr` (unique), `WeakRef<T>`. Use `static auto Create(...)` factories where the constructor is private or construction can fail (`Sampler`, `Shader`, `DeviceManager`).
- **`EP_ASSERT` is a `constexpr` function, not a macro**: `EP_ASSERT(cond, "message")`. Do not copy the older `EP_ASSERT(false && "msg")` form still present in a few files — the `&&` collapses to a plain `false` and the message is silently discarded.
- `EP_PROFILE_FN("Scope::Name")` as the first statement of a hot function, no trailing semicolon.

**Prose and error handling**

- Comments: zero is the default. Add one only for a non-obvious "why", max 1 line. Never restate what the code does. `///` doc comments are rare — reserved for public serialization/lifecycle APIs.
- On error paths, log via `Log::` rather than silently returning. Guard clauses with early return; no braces around single-statement bodies.
- Don't add synonym APIs — if equivalent functionality exists, point the caller at it.

## Domain skills

Seven domain skills live in `.agents/skills/` (each `SKILL.md` + `references/architecture.md`, plus an `agents/openai.yaml` agent definition). Read the matching skill before investigating or changing a major subsystem; use every applicable skill for cross-system work. There is no mirrored skill tree — `.agents/skills/` is the single source of truth, and the openai.yaml sits alongside its skill so both stay consistent.

Seven domain skills:

- `eppo-scripting-integration` — CoreCLR hosting, native/managed ABI, assemblies, ScriptGlue, fields, lifecycle, deployment, and scripting tests.
- `eppo-rendering-pipeline` — Vulkan/NVRHI devices, shaders, descriptors, GPU resources, render passes, SceneRenderer, and graphical tests.
- `eppo-editor-development` — EditorLayer state, edit/play transitions, panels, viewport input, gizmos, projects, scenes, and content browsing.
- `eppo-scene-ecs-lifecycle` — EnTT entities, UUIDs, relationships, transforms, copy/duplication, serialization, runtime systems, and scene tests.
- `eppo-physics-integration` — Box3D bodies, hierarchy-aware colliders, transform conversion, runtime synchronization, scripting, and physics tests.
- `eppo-assets-and-projects` — asset handles, registry persistence, paths, loading/import/export, project lifecycle, `Game.eppak` packaging, and content-browser coordination.
- `eppo-application-framework` — application/frame lifecycle, layers, windows, events, input, ImGui, startup order, the deployed runtime, and application harnesses.

### Agents and skills outside the repo

The repo skills sit alongside global, user-level definitions that are not committed here:

- **Subagents** (`build` primary, plus `planner`, `coder`, `reviewer`, `researcher`, `junior` subagents) are defined in the global opencode config `~/.config/opencode/opencode.json` and apply to any project. Their descriptions are deliberately project-agnostic — use them in this repo, and supply project facts through this file and the domain skills.
- **General workflow skills** live in `~/.agents/skills/` (each `SKILL.md` + `agents/openai.yaml`): `build-test-verification`, `git-worktree-workflow`, `plan-and-confirm`, `preserve-local-style`, `requesting-code-review`, `systematic-debugging`, `test-driven-development`. They are deliberately workflow-only — no project-specific commands; project facts (build/test commands, suite names, paths) belong in this file and the domain skills.
- **Generic opencode user skills** live in `~/.config/opencode/skills/`. Same-named entries there shadow `~/.agents/skills/` — when a name exists in both trees, the opencode copy is the one loaded.

## Workflow rules (required)

- **Discover worktrees first.** Before inspecting, editing, building, or testing, run `git worktree list` from the repository and identify the worktree that contains the task. Never assume the primary checkout is the target; use the selected worktree consistently for every command.
- **Plan before code.** For anything beyond a trivial change, write a plan first and confirm key decisions (including naming/layout choices) with the user before implementing.
- **Test-driven development.** Write the test first as `TEST(Suite, Name)` (Google Test) in the matching `EppoEngineTesting/Source/<module>/` file; a suite is just the shared first argument, so a new suite must also be registered in `Scripts/Premake/Testing.lua`. Use `EXPECT_*`/`ASSERT_*`, `EP_REQUIRE` for fatal checks inside value-returning helpers, and the `CHECK_VEC*_CLOSE` glm helpers. Name suites/tests after the class/behaviour under test, not the goal ("Smoke"/"Sanity" are banned). Critical bug fixes get a regression test.
- **Systematic debugging.** Root cause before fix; no patching symptoms.
- **Code review via subagent** after substantial changes — do not review your own work.
- **No formatting changes to existing code.** Don't reindent or reflow lines you aren't otherwise editing, and never run clang-format across a file you didn't create. New and edited lines use 4 spaces (see Style); the tab-to-space conversion of legacy files is a deliberate, separately-run pass, not something to do as a drive-by.
- **Verify before claiming done.** Run the relevant build + `ctest` and confirm it passes. A green build alone does not verify editor/GUI behaviour — state what was actually verified.
- **Be honest, not agreeable.** Do not reflexively agree with the user. If you think they are wrong, say so and explain why. Sycophancy ("you're right", "fair", "good point") without independent judgment is a failure mode. Disagreement must be substantive — do not manufacture contrarianism either. The user pays you to think, not to nod.
- **Don't invalidate the build cache by default.** The user's build cache is expensive to rebuild. Do not run `Scripts\Setup.bat`/`setup.sh` (re-provisions tools and re-runs `vcpkg install`), `Scripts\Clean.bat`/`clean.sh` (wipes all build outputs and provisioned tools), or `Scripts\GenerateBuildFiles.bat`/`generatebuildfiles.sh` (re-runs Premake) by default. Do not delete or touch the `build/` tree or `.eppo/` directly. Routine edits do not require regeneration; `GenerateBuildFiles` is only needed when premake inputs change (e.g., adding/removing/renaming files). If a cache-invalidating step is genuinely necessary, just do it.

## CI

GitLab CI (`.gitlab-ci.yml`): runs on MRs, `master`, `develop`, `feature/*`, `test/*`. On Linux it generates Ninja with Premake beta8, builds only `EppoEngineTesting_Debug_x64`, runs `ctest --label-exclude graphical`, and publishes JUnit. The toolchain is baked into `.gitlab/ci/Dockerfile`; the vcpkg binary cache is keyed on `vcpkg.json` + `vcpkg-configuration.json`. Use `glab` CLI for MR operations.
