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
| Packaging | `Project/GameData.*`, `Project/ProjectExporter.*`, `Asset/PackFormat.h` |
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

`AssetImporter` holds three dispatch tables keyed by `AssetType`: disk import, **packed** import, and export. Scene is implemented in all three via `SceneSerializer`; Mesh has disk import/export but **no packed importer**, so meshes cannot yet be loaded out of a package even though `PackFormat::Mesh` reserves a magic for them. A packed-mesh path needs the artifact model decided first (processed mesh data, not the glTF source) — do not wire a registration that would resolve to an unimplemented reader.

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

## Packaging to `Game.eppak`

`GameData` is the in-memory form of the package and the authority on its byte layout, which is documented as a field-by-field map in the header comment of `Project/GameData.h` — update that comment with any change. Every section is read and written through `Core/Buffer/` streams (`FileStreamWriter`/`FileStreamReader`), so a truncated or foreign file fails as a `false` return rather than an assert or a crash.

`Asset/PackFormat.h` holds the four-character magic + version pairs: `EPAK` (package), `ESHD` (shaders), `EMSH` (mesh), `ESCN` (scene). Bump the version of whichever payload you changed.

`ProjectExporter::Export` is a single ordered pass; `ProjectExportOptions` carries the configurations, build toggles and a progress callback, and `ProjectExportResult` accumulates warnings and errors instead of throwing:

1. `ValidateProject` — name, configurations, start scene, output path. Nothing touches the filesystem until it passes.
2. Pack scenes. A packed scene is the `.epscene` file's **raw bytes** carried as a `PackedAssetData` payload; runtime-generated assets are skipped.
3. Pack shaders from the live renderer (`GetAllShaders()` — their sources are already in memory), then walk `Resources/Shaders` for `.hlsli` includes, keyed by path relative to that directory because that is how the sources `#include` them. This walk must stay in step with `ReadIncludesFromDisk` in `VulkanShader.cpp`, which hashes the same set for the shader cache key.
4. Build (optional) and validate the standalone runtime per configuration.
5. Create the output tree. **From here on every failure wipes the partial export** through the local `fail()` helper — preserve that, a half-written game directory is worse than none.
6. Per configuration: compile the user's C# scripts into the output via `dotnet`, stage the runtime executable + native dependencies + managed core, copy the loose `Assets` tree, then `gameData.Serialize(outputDirectory / GameData::Filename)`.

Two consequences worth holding on to. First, packing shaders needs a live renderer, which is why exporter tests are graphical (`ProjectExport`) rather than unit. Second, gathering happens **inside** `Export` for every payload — do not add an option flag, constructor parameter, or externally-captured argument for one section, because that makes it the odd one out.

## Consuming the package

`EppoRuntime` deserializes `Game.eppak` inside `CreateApplication`, before the `Application` exists, then splits it: shaders and includes move into `ApplicationParams` (the renderer owns them from that point), and the rest goes to `RuntimeLayer`.

`AssetManager` has a packed constructor taking owned metadata and `PackedAssetData` payloads. In that mode `GetOrLoadAsset` deserializes lazily from the in-memory payload instead of reading disk. There is no `PackedAssetManager` class — `EppoEngineTesting/Source/Project/PackedAssetManager.cpp` exercises `AssetManager`'s packed mode, and its tests are named accordingly.

## Scene asset ownership

`Scene` derives from `Asset` and carries its handle. Saving a previously unregistered scene creates registry metadata using that existing handle, then loads/caches it. Editor active-scene paths and project start-scene handles must remain consistent when using Save As or opening by filesystem path versus handle.

## Testing strategy

The headless `Project` suite (`EppoEngineTesting/Source/Project/`) owns `GameData` round-trips, packed-asset loading through `AssetManager`, and registry/path behavior. The graphical `ProjectExport` suite owns `ProjectExporter`, because packing shaders reads them from a live renderer; each of its tests guards on `Testing::AppHarness::IsAvailable()` and returns early when no GPU is present.

Use `Testing::TempDir` for project directories and restore the previously active project after each test. Never mutate checked-in editor projects or their registries. Scene persistence belongs in `Scene`; end-to-end opening and rendering belongs in the `Renderer` suite's `SceneRendering` tests.
