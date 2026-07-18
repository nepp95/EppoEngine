# Editor architecture

## Ownership map

`EppoEditor.cpp` implements `CreateApplication` and pushes `EditorLayer`. `EditorLayer` is the editor shell and owns:

- `m_EditorScene`: the authored scene;
- `m_ActiveScene`: the scene currently displayed and updated;
- `m_SceneState`: edit or play;
- `SceneRenderer` and `EditorCamera`;
- `PanelManager`, toolbar icons, viewport state, gizmo state, and project/scene commands.

`PanelManager` owns panels and centralizes scene context plus selected `Entity`. Panels receive a non-owning manager pointer through `Panel`. Current panels are:

- `SceneHierarchyPanel`: tree display, selection, entity creation/deletion, and hierarchy interaction;
- `PropertyPanel`: component editing, script fields, component addition/removal, collider fitting;
- `ContentBrowserPanel`: filesystem navigation, asset icons, importing, moving/renaming/deleting, and opening scenes through a callback.

## Edit/play state machine

In edit state, `m_ActiveScene == m_EditorScene`. The editor camera renders the authored scene, selection highlighting is active, and editing commands operate on authored data.

Play transition:

1. Capture selected UUID before replacing the scene.
2. Set state to play.
3. `Scene::Copy(m_EditorScene)` into `m_ActiveScene`.
4. Update panel scene context.
5. Resolve selection by UUID in the runtime copy.
6. Start runtime physics and scripts.
7. Set script scene context to the runtime scene.

Play update steps runtime, then renders from the primary scene camera. If no primary camera exists, the editor camera renders as a fallback and the viewport displays a notice rather than a stale frame.

Stop transition:

1. Clear script scene context while the runtime scene is alive.
2. Stop the runtime scene.
3. Capture the selected runtime UUID before releasing the copied scene.
4. Restore `m_ActiveScene = m_EditorScene` and edit state.
5. Update panels and resolve selection by UUID in the authored scene.

Never retain an `Entity` across scene replacement: it contains an EnTT handle and raw `Scene*`.

## Per-frame ordering

`OnUpdate` consumes UI state from the previous UI pass. It:

1. Pulls selection from `PanelManager`.
2. Propagates viewport size to cameras, both scenes, and `SceneRenderer`.
3. Gates polled input using last frame's viewport-focus state.
4. Refreshes the renderer's scene reference and edit-mode highlight.
5. Updates the editor camera or runtime scene and renders.

`OnUIRender` applies deferred layout restoration before any windows begin, builds the dockspace/menu, renders the viewport image, records viewport bounds/focus/hover, draws toolbar/notices, updates panels, and handles popups.

One-frame lag for focus or selection is intentional where documented. Avoid mixing same-frame UI mutation into render-update state unless the ordering is deliberately redesigned.

## Project and scene flow

Opening a project:

1. Close/save the previous project and unload its collectible user assembly.
2. Deserialize the `.epproj` and asset registry.
3. Build the project C# assembly with the current `EppoScriptCore.dll` path.
4. Initialize scripting and load the user assembly.
5. Open the start scene only after script class metadata exists, so script fields deserialize correctly.

Saving a project saves the active scene, assigns the start scene if absent, serializes registered scenes, writes the asset registry, and writes the project file.

Opening scenes must go through `EditorLayer`, even when initiated in `ContentBrowserPanel`, because the layer owns active/editor scene bookkeeping and scripting assumptions.

## Panel extension checklist

To add a panel:

1. Derive from `Panel` and implement `RenderGui`.
2. Register it in `EditorLayer::OnAttach` through `PanelManager::AddPanel`.
3. Use manager-provided scene and selection instead of storing a divergent authoritative copy.
4. Add its window toggle to the editor menu.
5. If it belongs in the default dock layout, update `Resources/Layouts/DefaultLayout.ini` and keep its window name stable.
6. Load icons/resources from `FS::GetResourcesDirectory()`.

Use callbacks to request editor-authoritative operations rather than giving a panel broad access to `EditorLayer` internals.

Entity duplication is already routed from `SceneHierarchyPanel` to `Scene::DuplicateEntity`; extend the engine operation and its tests before adding editor-side duplication logic.

## Property editing checklist

When adding a component editor:

- Match the component's native units and coordinate conventions.
- Use `DrawComponent` for consistent header/removal behavior.
- Disable or guard operations that require another component or loaded asset.
- Route collider auto-fit to `Scene`, where runtime-independent mesh-bound logic belongs.
- For script fields, edit `ScriptEngine`'s stored field map, not a live runtime instance.
- Consider whether editing should be allowed in play mode and whether it should persist after stop.

## Content browser model

The content browser displays both registered and unregistered filesystem entries. `AssetManager::GetHandleForPath` determines registration. File mutations must coordinate filesystem state with registry state:

- import/create registers supported types;
- move/rename updates registered metadata paths;
- delete removes registry metadata and separately deletes the selected filesystem entry;
- opening a scene calls back into `EditorLayer` by handle;
- icons derive from `AssetType`, with generic file/directory fallbacks.

## Resources and sample project

`EppoEditor/Resources/` (shaders, fonts, icons, meshes, project templates) is synced incrementally to the executable output directory via a `_ResourcesSync` stamp file; resolve it at runtime through `FS::GetResourcesDirectory()`. Panels use the engine's `ImGui/ScopedBegin.h` and `ImExt.h` helpers; toolbar hit-testing goes through `Utils::IsInsideRoundedRect` so clicks in rounded-corner gaps are ignored. A sample project lives at `EppoEditor/Projects/Test/Test.epproj`.

## Testing boundaries

Prefer tests in engine suites for behavior extracted from editor UI: scene hierarchy, serialization, asset registry, collider fitting, scripting fields, and input semantics. Use `Scenario` for editor-camera and scene-render behavior over frames. `App` verifies real application/window/device boot. Direct editor UI automation is not currently part of the repository test harness, so keep UI handlers thin and engine behavior testable.
