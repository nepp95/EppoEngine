---
name: editor
description: Orientation for the Eppo editor app (EppoEditor) — EditorLayer, the ImGui dockable panels (Scene Hierarchy, Property, Content Browser), play/stop, projects & scenes, the toolbar, and Resources. Use when working on the editor UI, panels, EditorLayer, play mode, docking, or editor Resources.
---

# Editor sector

`EppoEditor/` is the ImGui editor **exe** that links `EppoEngine`. It's `main()`
via `EntryPoint.h` + a `CreateApplication` that pushes one `EditorLayer`.

## Map — `EppoEditor/Source/`

- **`EppoEditor.cpp`** — `CreateApplication`, pushes `EditorLayer`.
- **`EditorLayer.{h,cpp}`** — the heart. Owns:
  - **Two scenes**: `m_EditorScene` (authoritative, edited) and `m_ActiveScene`
    (what's live). `OnScenePlay()` copies editor→active and starts scripts;
    `OnSceneStop()` discards the play copy and restores. `m_SceneState` is
    `Edit`/`Play`.
  - `m_SceneRenderer` (draws `m_ActiveScene`), `m_EditorCamera`, `m_PanelManager`.
  - Project/scene file ops: `NewProject`/`OpenProject`/`SaveProject`,
    `NewScene`/`OpenScene`/`SaveScene`/`SaveSceneAs`.
  - UI: `UI_Toolbar` (play/stop icons), `UI_NewProjectPopup`, docking via
    `RestoreDefaultLayout` (deferred through `m_RestoreLayoutRequested` because
    layout restore must precede any window `Begin` that frame).
  - `m_MissingPrimaryCamera` — play falls back to the editor camera + a notice
    when the runtime scene has no primary camera.
- **`Panels/`** — managed by `PanelManager`; base class `Panel`:
  - `SceneHierarchyPanel` — entity tree, selection.
  - `PropertyPanel` — component editing (incl. script fields via `ScriptEngine`'s
    side table; editing during play mutates the live instance).
  - `ContentBrowserPanel` — asset/directory browsing, icons, drag-drop sources.

## Resources

`EppoEditor/Resources/` (shaders, fonts, icons, meshes, new-project templates) is
copied incrementally to the output dir via a stamp file (`_ResourcesSync`). The
app resolves them **relative to the working directory**, so the exe must run with
its own dir as cwd. Sample project: `EppoEditor/Projects/Test/Test.epproj`.

## Conventions

- Panels are ImGui immediate-mode; use `ImGui/ScopedBegin.h` and `ImExt.h`
  helpers from the engine. Toolbar hit-testing uses `Utils::IsInsideRoundedRect`
  so clicks in the rounded-corner gaps are ignored.
- Editor-only camera work belongs to `EditorCamera`; play-mode view comes from a
  `CameraComponent` with `Primary` set.

## Build, run & verify

Editor/UI changes **must be seen** — a green build proves nothing about the UI.
Use `/run-eppo-editor` to build and launch, then interact. Key points from that
skill: launch with the exe dir as cwd; on startup a native Open dialog blocks —
pick `Test.epproj` or cancel for an empty scene. Crash logs: `latest.log` /
`previous.log` next to the exe.

```powershell
cmake --build --preset=windows-debug
Start-Process -FilePath .\build\debug\EppoEditor\Debug\EppoEditor.exe `
              -WorkingDirectory .\build\debug\EppoEditor\Debug
```
