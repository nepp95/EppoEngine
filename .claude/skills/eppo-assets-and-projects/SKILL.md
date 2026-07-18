---
name: eppo-assets-and-projects
description: Develop and diagnose Eppo asset and project workflows across project lifecycle, asset handles and metadata, registry persistence, relative path normalization, lazy loading, import/export dispatch, generated runtime assets, scene ownership, ContentBrowserPanel operations, project templates, and asset-related serialization. Use for changes under EppoEngine/Source/Asset, EppoEngine/Source/Project, EppoEditor/Source/Panels/ContentBrowserPanel, project templates, or asset registry behavior.
---

# Eppo Assets and Projects

Read [references/architecture.md](references/architecture.md) before changing handles, paths, registry persistence, importers, or content-browser mutations. Treat the file on disk, registry metadata, loaded object, and editor presentation as distinct states.

## Workflow

1. Identify which identity is authoritative: project path, asset-relative path, stable `AssetHandle`, loaded `Asset`, or generated reserved handle.
2. Define disk and registry effects before editing. Keep move, rename, delete, import, and save operations consistent across both.
3. Add or extend engine APIs for reusable behavior; keep file-picker and ImGui orchestration in the editor.
4. Update type deduction, importer/exporter dispatch, icons, serialization, and opening behavior together when adding an asset type.
5. Preserve active-project preconditions and avoid holding references across project close or replacement.
6. Test path/registry logic headlessly where practical; use scenario/editor testing only for UI-to-engine coordination.

## Guardrails

- Store asset paths relative to the active project's `Assets` directory.
- Reserve handle `0` as null and low handles for generated runtime primitives.
- Never delete a source file merely by removing registry metadata.
- Serialize registry mutations after releasing its mutex.
- Do not treat a registered asset as necessarily loaded, or a filesystem entry as necessarily registered.
