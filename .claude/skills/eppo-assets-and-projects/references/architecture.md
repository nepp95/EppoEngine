# Asset and project architecture

## Core identities

Keep four states distinct:

1. A filesystem entry under a project's `Assets` directory.
2. `AssetMetadata` in `AssetRegistry.json` with handle, type, and relative path.
3. A loaded `Asset` object in `AssetManager::m_LoadedAssets`.
4. An editor representation in `ContentBrowserPanel`.

A file may be unregistered. Registered metadata may be unloaded. A generated asset may have no file or serialized registry entry.

`AssetHandle` is a UUID value. Handle `0` is null. Reserved low values generate built-in mesh primitives at runtime; normal imported assets use generated UUIDs.

## File map

| Area | Files |
| --- | --- |
| Asset base/types | `Asset/Asset.h`, `AssetType.h`, `AssetMetadata.h` |
| Registry/cache | `Asset/AssetManager.*` |
| Dispatch | `Asset/AssetImporter.*` |
| Project context | `Project/Project.*`, `ProjectSerializer.*` |
| Scene asset format | `Scene/SceneSerializer.*` |
| Editor filesystem UI | `EppoEditor/Source/Panels/ContentBrowserPanel.*` |
| Templates | `EppoEditor/Resources/Templates/NewProject` |

## Project lifecycle

`Project` holds `ProjectSpecification` and one `AssetManager`; `Project::s_ActiveProject` is the global project context used by path and asset APIs.

Opening a project deserializes the `.epproj`, sets its directory, publishes it as active, constructs the asset manager, and loads `Assets/AssetRegistry.json`. The editor then builds/loads scripts and opens the start scene.

Saving serializes registered scene assets, the asset registry, and the project specification. Closing saves, unloads the user assembly, clears active editor scenes, and releases the active project.

Functions such as `GetAssetsDirectory` assert an active project. Guard UI/background paths that can run during startup, failed open, or close.

## Path contract

- Project file: `<ProjectDirectory>/<ProjectName>.epproj`.
- Assets root: `<ProjectDirectory>/Assets`.
- Scripts root: `<ProjectDirectory>/Scripts`.
- Registry metadata stores paths relative to `Assets`.
- `Project::GetAssetFilepath` maps metadata to disk.
- `Project::GetAssetRelativeFilepath` normalizes absolute editor selections before registration.

Normalize at API boundaries. Do not compare an absolute content-browser path directly with stored relative metadata.

## Registry and loading

`CreateAsset` deduces type from extension, assigns the existing object's handle or a new UUID, inserts metadata under a mutex, then serializes the registry. `GetOrLoadAsset` returns cached objects, generates reserved primitives, or invokes the importer selected by metadata type.

`RemoveAsset` removes metadata and any loaded cache entry, then serializes. It does not delete the source file. `UpdateAssetPath` changes metadata after a disk move/rename and then serializes.

Registry serialization skips empty paths and runtime-generated assets. Release the registry lock before filesystem writes to avoid extending critical sections or deadlocking through future callbacks.

The asynchronous loading parameter and `Tick` are currently scaffolding; do not claim async loading works without implementing synchronization, completion publication, and tests.

## Import/export dispatch

`AssetImporter` maps `AssetType` to import/export functions. Scene import/export is implemented through `SceneSerializer`. Mesh import/export entries exist but are currently incomplete; mesh construction elsewhere can load glTF directly.

Adding an asset type normally requires:

1. Add enum/string conversions and extension deduction.
2. Add metadata/import/export dispatch.
3. Implement the actual asset class and loader.
4. Add content-browser icon and open behavior.
5. Add serialization/reference behavior for consumers.
6. Add registry round-trip and load tests.

Do not register an extension as supported if its importer always returns null.

## Content browser coordination

The content browser synchronizes its root/current directory when the active project changes. It displays directories and files, assigns icons by registered or inferred type, and provides import/open/move/rename/delete operations.

Mutation sequence matters:

- Move/rename on disk first only if failure can be handled; then update metadata for registered assets.
- Delete the selected disk path and remove metadata when registered; do not conflate the two operations.
- Import external files into the project assets tree before registering the project-relative destination.
- Open scenes through the callback owned by `EditorLayer`, not by replacing panel context locally.

## Scene asset ownership

`Scene` derives from `Asset` and carries its handle. Saving a previously unregistered scene creates registry metadata using that existing handle, then loads/caches it. Editor active-scene paths and project start-scene handles must remain consistent when using Save As or opening by filesystem path versus handle.

## Testing strategy

No dedicated Asset suite exists yet. Put reusable registry/path tests in a matching new engine suite or an appropriate core/scene suite and register it with `AddTestingSuite`. Use temporary project directories and restore active-project state after each test. Avoid tests that mutate checked-in editor projects or their registries. Scene persistence belongs in `Scene`; end-to-end opening/rendering can use `Scenario`.
