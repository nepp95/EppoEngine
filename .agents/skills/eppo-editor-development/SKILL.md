---
name: eppo-editor-development
description: Extend and diagnose EppoEditor workflows across EditorLayer, edit/play scene state, panels and shared selection, viewport rendering and input focus, gizmos, project and scene commands, content browsing, docking, editor resources, and editor-to-engine boundaries. Use for changes under EppoEditor/Source or EppoEditor/Resources and for engine APIs introduced specifically to support editor behavior.
---

# Eppo Editor Development

Read [references/architecture.md](references/architecture.md) before changing `EditorLayer` or a panel. Decide first whether the behavior belongs in the reusable engine or only in the editor shell.

## Workflow

1. Place reusable scene, asset, physics, scripting, or rendering behavior in `EppoEngine`; keep orchestration and authoring UI in `EppoEditor`.
2. Trace editor state through `m_EditorScene`, `m_ActiveScene`, `SceneState`, `PanelManager` scene context, selection, and `SceneRenderer` scene context.
3. Preserve UUID-based remapping whenever a scene copy or replacement invalidates EnTT handles and `Entity` wrappers.
4. Route panel-wide scene context and selection through `PanelManager`. Route operations requiring editor authority, such as opening a scene, back through `EditorLayer` callbacks.
5. Keep polled input gated by viewport focus and keep gizmo interaction from also moving the editor camera.
6. Test extracted engine behavior in its matching headless suite. Use `App` for real boot and frame advancement, or the `Renderer` suite's `SceneRendering` tests when the change requires the real renderer, viewport, or editor-camera path.

## Guardrails

- Start play from a copy of the editor scene; never mutate the authored scene as runtime state.
- Stop runtime and clear script contexts before dropping the runtime scene.
- Resolve runtime assets and UI resources relative to the executable working directory.
- Apply docking-layout restoration before submitting windows for the frame.
- Preserve panel names referenced by the default docking layout.
