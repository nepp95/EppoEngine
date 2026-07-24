# Adding an asset type

An asset type = an entry in the `AssetType` enum plus a `Ref<Asset>` subclass the
importer produces. On this branch assets also have a **packed** form that ships
inside `Game.eppak` for the standalone runtime, so a fully runtime-shippable type
touches the pack path too. Grep the named symbols; line numbers drift.

## Engine — editor/loose path (`EppoEngine/Source/Asset`)

1. **`AssetType.h`** — add the enum value, and add both the `AssetTypeFromString`
   and `AssetTypeToString` cases. Both `EP_ASSERT` on unknown types, so a missing
   case fails at registry load/save, not compile.

2. **The asset class** — derive from `Asset` (see `Renderer/Mesh.h`,
   `Scene/Scene.h`) and give it `static auto GetStaticType() -> AssetType`. It
   carries `AssetHandle Handle` from the base.

3. **`AssetManager::GetAssetTypeFromPath`** (`AssetManager.cpp`) — map the file
   extension(s) to the new type. The content browser and `CreateAsset` classify
   files through it. Runtime-generated types with no file skip this — see
   `GenerateAsset` and the reserved-handle (`id < 100`) path.

4. **`AssetImporter`** (`AssetImporter.h` + `.cpp`) — add `ImportXxx` (and
   `ExportXxx` if savable) and register them in the `s_AssetImportFns` /
   `s_AssetExportFns` maps at the top of the `.cpp`. `ImportAsset`/`ExportAsset`
   log and return null for unregistered types — silent at compile, loud at
   runtime.

## Engine — packed/runtime path (only if the type ships in `Game.eppak`)

Skip this entire section for editor-only asset types. Scenes are the reference
implementation.

5. **`PackFormat.h`** — add a per-asset `FormatId` (four-char magic + version) and
   a `case` in `PayloadFormat` returning it. `std::nullopt` means "not packable
   yet"; the exporter uses this to decide what it can pack.

6. **`AssetImporter`** — add `ImportPackedXxx(handle, BufferReader&)` and register
   it in `s_AssetImportPackedFns`. This is the `AssetImporter::ImportAsset(handle,
   type, reader)` overload the runtime's asset manager calls; a missing entry logs
   and returns null.

7. **The asset's binary (de)serializer** — the payload written by the exporter
   must be readable by step 6. `Scene` does this with
   `SceneSerializer::Serialize/Deserialize(BufferWriter&/BufferReader&)` guarded
   by `PackFormat::Scene` magic+version. New types need an equivalent using
   `Core/BufferReader`/`BufferWriter`.

8. **`ProjectExporter::Export`** (`Project/ProjectExporter.cpp`) — the pack loop
   currently filters to `AssetType::Scene` (`if (metadata.Type != AssetType::Scene
   ...) continue`). To ship the new type, extend that loop to serialize it into a
   `PackedAssetData` and `gameData.PackedAssets.emplace(handle, …)`. Note
   `CopyAssets` deliberately drops `.epscene` + `AssetRegistry.json` from the
   loose tree because they live in `Game.eppak` — decide whether the raw file for
   your type should ship loose or packed.

## Editor (`EppoEditor/Source/Panels`)

9. **`ContentBrowserPanel`** — add an icon field + `load(...)` in `LoadIcons`, a
   `case` in `GetIcon`, and a `Resources/Icons/<Type>.png`. If it is drag-and-drop
   importable into a scene, extend `IsImportable`.

10. **`PropertyPanel`** — only if an entity references the asset by handle (as
    `MeshComponent` does): add the type guard where the asset picker filters by
    `GetMetadata(handle).Type`.

## Scripting (only if scripts reference the asset)

11. The `MeshComponent` mesh-handle path (`MeshComponent_*` + `AssetType::Mesh`
    guard in `ScriptGlue.cpp`) is the template. See
    [adding-a-script-internal-call.md](adding-a-script-internal-call.md).

## Verify

Build `EppoEngineTesting` and run `ctest -R Scene` (+ `Project` /
`PackedAssetManager` / `ProjectExporter` if you touched the pack path). Then
import a file of the new type through the content browser, confirm it lands in
`AssetRegistry.json` with the right `Type` string and reloads; if packable,
export the project and confirm the runtime loads it from `Game.eppak`.
