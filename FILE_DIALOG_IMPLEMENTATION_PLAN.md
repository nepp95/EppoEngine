# File Dialog Implementation Plan — ImGui-based file dialog in the editor

## Context

The editor currently opens files/folders through `Eppo::FileDialog`
(`EppoEngine/Source/ImGui/FileDialog.{h,cpp}`), a thin wrapper over the
`nativefiledialog-extended` (NFD) vcpkg overlay port. NFD spawns the OS-native
dialog, which means platform-specific behaviour, an extra native dependency, and
a UX that doesn't live inside the engine's own docking UI.

Goal: replace the native dialog with an in-engine **Dear ImGui** dialog so it is
uniformly cross-platform (one code path on Windows/Linux), lives inside the
editor's own theme/dockspace, and is fully under our control. NFD is removed
outright — a single dialog system, nothing platform-specific left to maintain.

Chosen approach:
- **Vendor `aiekick/ImGuiFileDialog`** (MIT, tracks the ImGui docking branch,
  feature-rich: filters, folder + save modes, bookmarks) rather than hand-rolling.
- **Remove `nativefiledialog-extended` entirely** — overlay port, CMake wiring,
  `Window` init/quit, and the `EnableFileDialogs` spec flag.
- Keep the public facade named `FileDialog` in the same module so the umbrella
  header (`EppoEngine.h`) and call sites need minimal churn.

### The one unavoidable architectural change

The current API is **blocking**: `FileDialog::OpenFile(...)` returns a
`std::filesystem::path` immediately. An ImGui dialog is immediate-mode and
renders across many frames, so it **cannot** return synchronously. Every call
site must move to a **request + callback** model. This is inherent to any
in-ImGui dialog, independent of the library chosen.

## Dependency integration (vcpkg overlay port)

ImGuiFileDialog is not in the vcpkg registry, so add it the house way — an
overlay port under `CMake/Ports/imguifiledialog/` (mirrors the existing `nvrhi`
and `nativefiledialog-extended` overlay ports). No `builtin-baseline` bump is
needed (overlay ports are independent of the baseline), which avoids version-scheme
conflicts and a forced full reconfigure.

- `CMake/Ports/imguifiledialog/vcpkg.json` — name/version/license (MIT),
  `dependencies`: `imgui` (must resolve to the **same** `imgui` 1.92.7 the engine
  uses, so it compiles against identical headers), plus `vcpkg-cmake` /
  `vcpkg-cmake-config` host tools.
- `CMake/Ports/imguifiledialog/portfile.cmake` — `vcpkg_from_github` at a pinned
  ref + SHA512, `vcpkg_cmake_configure` pointing the upstream CMake at the vcpkg
  imgui include dir, `vcpkg_cmake_install`, config fixup, copyright.
- ImGuiFileDialog reads a compile-time `ImGuiFileDialogConfig.h`. Start with the
  upstream default (std::filesystem backend, text labels — the engine has **no**
  icon font, so leave icon glyphs off for now). A custom config header for engine
  labels/icons is a later polish, not part of this branch.

Manifest + CMake edits:
- `vcpkg.json` — add the `imguifiledialog` dependency; **remove**
  `nativefiledialog-extended`.
- `CMake/Dependencies.cmake` — add `find_package(ImGuiFileDialog CONFIG REQUIRED)`
  (next to the existing imgui/imguizmo blocks); **remove** the
  `find_package(nfd REQUIRED)` block.
- `EppoEngine/CMakeLists.txt` — replace `nfd::nfd` in the PUBLIC link list with
  the ImGuiFileDialog target.
- **Delete** `CMake/Ports/nativefiledialog-extended/`.

## The `FileDialog` facade (async rewrite)

Rewrite `EppoEngine/Source/ImGui/FileDialog.{h,cpp}` from a blocking NFD wrapper
into an async facade over ImGuiFileDialog's keyed singleton. Keeps
`ImGuiFileDialog::Instance()` calls out of every call site.

```cpp
// FileDialog.h  (namespace Eppo)
class FileDialog
{
public:
    using ResultCallback = std::function<void(const std::filesystem::path&)>;

    // Each queues a dialog keyed by `key`; `onSelect` fires from Render() when the
    // user confirms. Cancel does nothing. `filters` uses ImGuiFileDialog syntax
    // (e.g. ".epscene", or "Importable{.gltf,.glb,.png}"); folder pick passes none.
    static auto OpenFile(std::string key, std::string title, std::string filters,
                         std::filesystem::path initialDir, ResultCallback onSelect) -> void;
    static auto SaveFile(std::string key, std::string title, std::string filters,
                         std::filesystem::path initialDir, ResultCallback onSelect) -> void;
    static auto OpenFolder(std::string key, std::string title,
                           std::filesystem::path initialDir, ResultCallback onSelect) -> void;

    // Called once per frame; displays any open dialog and dispatches callbacks.
    static auto Render() -> void;
};
```

- Internally keep a small registry (`key -> ResultCallback`) in an anonymous
  namespace (per style rules), open via `ImGuiFileDialog::Instance()->OpenDialog`
  with a modal config centered like the other editor popups, and in `Render()`
  call `Display(key)` → on `IsOk()` fire the callback with `GetFilePathName()` /
  `GetCurrentPath()` (folder), then `Close()`.
- Preserve the existing **"resolve to nearest existing directory"** guard
  (`Utils::ExistingDirectory` in the current `FileDialog.cpp`) — still needed so a
  non-existent initial dir doesn't misbehave.
- Trailing return types, `m_`/`s_` naming, guard-clause early returns, log on
  error via `Log::` — per CLAUDE.md style.

`FileDialog::Render()` is invoked **once**, in `EditorLayer::OnImGuiRender`
alongside the existing popup block. Because the facade is global, this single call
also drives the ContentBrowser's import dialog — no per-panel render hook needed.

## Call-site conversion (5 sites)

Convert each blocking call to a request + callback. Move any post-selection work
into the callback.

| Site | Location | New shape |
|------|----------|-----------|
| Open Project | `EditorLayer.cpp` (~581) | `OpenFile("OpenProject","Open Project",".epproj",Project::GetProjectsDirectory(),[this](auto p){ OpenProject(p); })` |
| Export Game (folder) | `EditorLayer.cpp` (~654) | `OpenFolder("ExportGame","Export To", parentDir, [this](auto p){ /* set options.ParentDirectory, launch async Export */ })` |
| Open Scene | `EditorLayer.cpp` (~711) | `OpenFile("OpenScene","Open Scene",".epscene",Project::GetAssetsDirectory(),[this](auto p){ OpenScene(p); })` |
| Save Scene As | `EditorLayer.cpp` (~787) | `SaveFile("SaveSceneAs","Save Scene As",".epscene",Project::GetAssetsDirectory(),[this](auto p){ m_ActiveScenePath=p; Serialize... })` |
| Import File | `ContentBrowserPanel.cpp` (~444) | `OpenFile("Import","Import File","Importable{.gltf,.glb,.epscene,.png,.jpg,.jpeg,.tga,.bmp,.hdr}", m_CurrentDirectory, [this](auto p){ ImportFile(p); })` |

**Return-value audit (required before editing):** `OpenScene()`, `SaveScene()`, and
`SaveSceneAs()` currently return `bool`. With the async model that boolean can no
longer signal the user's eventual choice. Trace their callers (`SaveScene` falls
through to `SaveSceneAs`; File-menu wiring in `EditorLayer.cpp`) and confirm none
gate later work on a synchronous result. Where they do (e.g. "save then X"), the
follow-up work moves into the callback. Keep the `path`-taking overloads
(`OpenScene(const path&)`, etc.) synchronous — only the no-arg dialog-driving
variants become async/void.

## NFD removal cleanup

- `EppoEngine/Source/Core/Window.cpp` — remove `NFD::Init()`, `NFD::Quit()`, and
  the `<nfd.hpp>` / `<nfd_glfw3.h>` includes.
- `EppoEngine/Source/Core/Window.h` — remove `EnableFileDialogs` and
  `m_FileDialogsInitialized`. The flag becomes vestigial (ImGuiFileDialog needs no
  global init).
- Update the configs that set the flag: `EppoRuntime/Source/EppoRuntime.cpp` and
  `EppoEngineTesting/Source/App/App.cpp` (three sites) — drop the field.
- Remove `nfdfilteritem_t` usage (only lived in the wrapper + call sites).

## Testing / TDD

GUI dialogs render across frames and need a display+GPU, so the dialog UI itself
isn't unit-testable headlessly. The genuinely testable seam is the **pure filter
mapping** — a small helper that turns our display-name filters into ImGuiFileDialog
filter strings. Write that helper test-first (RED→GREEN) in a suite under
`EppoEngineTesting/Source/`, named after the behaviour (e.g. `FileDialogFilter`),
covering single-ext, multi-ext grouped, and folder (empty) cases.

CI parity: CI builds `EppoEngineTesting` and runs `--label-exclude graphical`.
After NFD removal the engine + test runner must still **configure, build, and link**
with no `nfd` reference. That compile-and-link check is the primary regression guard
for the removal.

## Verification (end to end)

1. `cmake --preset windows-debug` — reconfigure; confirm vcpkg resolves the new
   `imguifiledialog` overlay port and no longer pulls `nativefiledialog-extended`.
2. `cmake --build --preset windows-debug` — full build (engine, editor, runtime,
   tests) links cleanly without `nfd`.
3. `ctest --test-dir build/debug --label-exclude graphical` — headless suites (incl.
   the new filter test) pass; this is what CI runs.
4. Launch the editor (run from `build/debug/EppoEditor`, capture startup log) and
   manually exercise all five flows: Open Project, Open Scene, Save Scene As, Export
   Game (folder), Import File — verify the ImGui dialog appears in-editor (themed,
   docked/modal), honours the initial directory and filters, and that confirm/cancel
   behave.
5. Code review via a `code-reviewer` subagent after implementation — do not
   self-review.

## Appendix: recommended future ImGui extensions (not in this branch)

- **ImPlot** — GPU-accelerated 2D plotting. Fit: an in-editor profiler/metrics panel
  (frame time, memory, Tracy-style live graphs). MIT; vcpkg port exists (`implot`),
  so no overlay needed.
- **Dear ImGui Test Engine** — automated UI/regression testing driving the editor's
  ImGui, plus a **command-palette** widget for editor actions. Fit: guards editor UX
  against regressions (complements the headless ctest suites); the command palette
  speeds up editor workflows. Note licensing (Test Engine is free for individuals/small
  teams; check terms) before adopting.

Both are additive and independent of this file-dialog work.
