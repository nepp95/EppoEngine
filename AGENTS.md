# AGENTS.md

Compact guide for OpenCode agents working in this repo. Read before editing.

## Project

EppoEngine — a C++20 cross-platform (Windows/Linux) game engine + editor with C# scripting via CoreCLR (.NET 10) and Vulkan rendering through NVRHI. Built with CMake + vcpkg (manifest mode).

## Prerequisites (all required, enforced by `CMake/Dependencies.cmake`)

- **Vulkan SDK** with `dxc` (`find_package(Vulkan REQUIRED COMPONENTS dxc)`). Set `VULKAN_SDK`.
- **vcpkg** with `VCPKG_ROOT` set and on PATH. Manifest mode; overlay ports in `CMake/Ports` (box3d, nvrhi, epposcriptcore).
- **.NET SDK 10** with `DOTNET_ROOT` set. Managed scripting core targets `net10.0`.
- **clang**: Windows presets pin `clang-cl`; Linux presets pin `clang/clang++`. MSVC alone is not used.
- Linux also needs X11/Wayland/GL dev libs (see `README.md`).

## Commands

Presets are platform-prefixed: `windows-debug` / `linux-debug` (Debug), `*-release` (RelWithDebInfo), `*-dist` (Release). Binary dirs: `build/debug`, `build/release`, `build/dist`.

```bash
cmake --preset windows-debug                                       # configure (linux-debug on Linux)
cmake --build --preset windows-debug                               # build everything
cmake --build --preset windows-debug --target EppoEngineTesting    # test runner only (fastest; what CI builds)
ctest --test-dir build/debug --output-on-failure                   # all suites
ctest --test-dir build/debug --label-exclude graphical             # headless only (what CI runs)
ctest --test-dir build/debug -R Scripting                          # one suite by name
```

Run a suite directly from the exe output dir (see Gotchas): `./EppoEngineTesting Scripting`.
Suite names: `Physics`, `Scripting`, `ScriptMarshalling`, `UUID`, `App`, `Scenario`. Labels: `unit`, `scripting`, `core`, `graphical`.

Required order: **configure -> build -> test**. After editing C# only, rebuild the `EppoEngineTesting` (or `EppoEditor`) target so the dotnet custom commands re-run and DLLs are re-copied.

## Layout

- `EppoEngine/` — static library, the engine. `Source/` modules: `Asset`, `Core`, `Event`, `ImGui`, `Physics`, `Platform`, `Project`, `Renderer`, `Scene`, `Scripting`, `Utility`. Public umbrella header `Source/EppoEngine.h`. PCH: `Source/pch.h`.
- `EppoEditor/` — editor executable (`EppoEditor.cpp` -> `EditorLayer`). Depends on `EppoEngine` + `EppoScriptCore`. Owns `Resources/` and `runtimeconfig.json`.
- `EppoScriptCore/` — C# class library (net10.0) built by `dotnet` via `CMake/Dotnet.cmake` (not msbuild). Produces `EppoScriptCore.dll` at the binary root.
- `EppoEngineTesting/` — UnitTest++ test runner. `Source/` suites mirror engine modules; `Support/` has `AppHarness` (graphical) and `TestContext` (scenarios). `TestData/Scripts/` builds the `EppoTesting.Scripts.dll` harness the Scripting suite loads. Suites registered via `AddTestingSuite` in its `CMakeLists.txt`.
- `EppoRuntime/` — scaffolded, currently **not wired into the top-level build** (no `add_subdirectory`); `BUILD_RUNTIME` option is unused.
- `CMake/` — `Dependencies.cmake`, `Dotnet.cmake` (managed-core build + `CopyBuildScripts` helper), `Ports/` vcpkg overlays.

Runtime behavior and reusable game functionality belong in `EppoEngine`, not `EppoEditor`. Editor code may call engine APIs, but must not own logic that a standalone game or runtime needs, such as deriving collider values from mesh bounds.

## Gotchas

- **Run editor/tests from the exe output dir.** They resolve `runtimeconfig.json`, `EppoScriptCore.dll`, and `Resources/` relative to the working directory. `VS_DEBUGGER_WORKING_DIRECTORY` is set accordingly; from a terminal, `cd` to the exe dir first or scripting fails to load.
- **Managed core is built by CMake, not your IDE.** `EppoScriptCore.dll` + `EppoTesting.Scripts.dll` are produced by `dotnet` custom commands and copied beside the exes (`CopyBuildScripts`, `_HarnessDeploy`). A plain `dotnet build` won't wire them into the C++ build.
- **Graphical suites (`App`, `Scenario`) need a real display + GPU.** They early-return if `AppHarness` can't boot. On headless/CI, always use `--label-exclude graphical`.
- **CoreCLR initializes once per process.** The Scripting suite shares one `ScriptEngine::Init` + user-assembly load; do not re-init the runtime per test.
- **clangd** reads `build/clangd` for `compile_commands.json` (`.clangd`), but presets emit it at `build/<preset>` (e.g. `build/debug`). Symlink/copy `build/debug/compile_commands.json` to `build/clangd` for clangd to work.
- **Platform/config macros:** `EP_PLATFORM_WINDOWS`/`EP_PLATFORM_LINUX`; `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`; `TRACY_ENABLE` in Debug and RelWithDebInfo. Linux defines `__EMULATE_UUID` (DXC cross-platform UUID path).

## Style

- `.clang-format`: 4-space indent, 140-col limit, Allman braces (custom `BraceWrapping`), pointer left (`int* p`), `SortIncludes: Never`, namespace indentation `All`. Run clang-format before committing.
- `.clang-tidy`: `bugprone-*`, `clang-diagnostic-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `misc-use-anonymous-namespace`, `misc-const-correctness`.
- Comments: concise or none. Max 1 line, only when it explains a non-obvious "why." Zero is the default — a getter obviously returns the thing you're getting; commenting that is noise. Never restate what the code does.

## CI

GitLab CI (`.gitlab-ci.yml`): runs on MRs, `master`, `develop`, and `feature/*` + `test/*` branches. `build-and-test` configures `linux-debug`, builds only the `EppoEngineTesting` target, runs `ctest --label-exclude graphical`, publishes JUnit. Toolchain baked in `.gitlab/ci/Dockerfile` (clang, CMake >= 3.31, Ninja, vcpkg, Vulkan SDK, .NET 10). vcpkg binary cache keyed on `vcpkg.json` + `CMakePresets.json`.

## Workflow rules (required)

- **Discover worktrees first.** Before inspecting, editing, building, or testing, run `git worktree list` from the repository and identify the worktree that contains the task. Never assume the primary checkout is the target; use the selected worktree consistently for every command.
- **Plan before code.** For anything beyond a trivial change, use the `plan-and-confirm` skill to write a complete plan, verify it works end-to-end, and confirm key decisions with the user. Do not write implementation code until the plan covers the entire task.
- **Test-driven development and regressions.** Use the `test-driven-development` skill: write the test first, then implement. New behavior gets a test in the matching `EppoEngineTesting/Source/<module>/` suite, or a new suite via `AddTestingSuite` in `EppoEngineTesting/CMakeLists.txt`. Critical bug fixes get a test that would have caught the defect when practical.
- **Systematic debugging.** Use the `systematic-debugging` skill. Find the root cause before attempting a fix; do not patch symptoms or proceed without evidence for the cause.
- **Code review via subagent.** After completing a substantial change, use the `requesting-code-review` skill to dispatch a fresh `general` subagent and act on its feedback. Do NOT review your own work.
- **No formatting changes to existing code.** Use the `preserve-local-style` skill. Do not reformat, reindent, or "clean up" code you are not otherwise editing. If you touch a line for a semantic reason, match the surrounding whitespace exactly; never run clang-format on files you didn't create. If `.clang-format` conflicts with the repo's actual style (for example, tabs versus spaces), match the file.
- **Git worktrees.** Use the `git-worktree-workflow` skill for isolated feature work where applicable. If asked to work directly on existing changes, preserve them and work in place.
- **Verify before claiming done.** Use the `build-test-verification` skill. Run the relevant build and `ctest` (or target suite) and confirm it passes before declaring success.
