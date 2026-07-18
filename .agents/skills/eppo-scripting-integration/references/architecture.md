# Scripting architecture

## File map

| Area | Primary files | Responsibility |
| --- | --- | --- |
| Runtime host | `EppoEngine/Source/Scripting/RuntimeHost.*`, `Platform.h`, vendored `hostfxr.h` and `coreclr_delegates.h` | Locate hostfxr, load it, initialize from `runtimeconfig.json`, and resolve unmanaged entry points. |
| Managed assembly facade | `Assembly.*`, `ManagedFunctions.h` | Resolve the exported managed function table, register native internal calls, load/unload the user assembly, and cache reflected metadata. |
| Engine lifecycle | `ScriptEngine.*`, `ScriptInstance.*`, `ScriptClass.*`, `ScriptField.h` | Own the core assembly facade, live entity instances, editor field storage, and active scene/physics contexts. |
| Native callbacks | `ScriptGlue.*` | Implement callbacks invoked by managed public APIs and publish the name/function table consumed by `Assembly::RegisterInternalCalls`. |
| Managed bridge | `EppoScriptCore/Source/Core/ScriptGlue.cs`, `InternalCalls.cs` | Export unmanaged entry points, discover user types, own managed instances, marshal calls, and store registered native pointers. |
| Public C# API | `EppoScriptCore/Source/Scene`, `Physics`, `Core`, `Math` | Present user-facing entities, components, input, logging, physics, key codes, and blittable vector types. |
| Build/deploy | `CMake/Dotnet.cmake`, target CMake files, `EppoEditor/runtimeconfig.json` | Build `EppoScriptCore.dll`, build the test user assembly, and copy managed outputs beside native executables. |
| Tests | `EppoEngineTesting/Source/Scripting`, `TestData/Scripts` | Exercise reflection, lifecycle, fields, method invocation, internal calls, exceptions, and layout. |

## Boot and assembly flow

1. `EditorLayer::OpenProject` builds the project's C# assembly before opening its start scene.
2. `ScriptEngine::Init(runtimeConfigPath)` constructs the singleton and its `Assembly`.
3. `Assembly` constructs `RuntimeHost`, resolves `EppoScriptCore.ScriptGlue` exports, bootstraps managed state, and registers every native internal call by name.
4. `LoadUserAssembly` enters a collectible managed load context, discovers non-core subclasses of `Eppo.Scene.Entity`, and rebuilds native `ScriptClass` metadata.
5. Scene deserialization can then restore editor-time field storage against available field metadata. It does not reject unknown class names; the property panel and runtime instance creation report class validity later.

CoreCLR is process-global in practice. `ScriptEngine::Shutdown` ends engine ownership, but tests must share one initialization rather than repeatedly booting CoreCLR.

## Runtime entity flow

`EditorLayer::OnScenePlay` copies the authored scene. `Scene::OnRuntimeStart` creates physics, then calls `ScriptEngine::OnCreateEntity` for each `ScriptComponent`. Creation resolves the class index, creates a managed instance keyed by entity UUID, copies serialized editor fields into it, and invokes `OnCreate`.

Current limitation: `EditorLayer` publishes `ScriptEngine`'s scene context only after `Scene::OnRuntimeStart` returns. Consequently, entity/component internal calls made from managed `OnCreate` cannot resolve the runtime scene. Treat any ordering change as a lifecycle fix requiring a regression; do not assume `OnCreate` currently has a usable scene context.

Each runtime update steps physics first and then invokes `OnUpdate`. On stop, `EditorLayer` clears the managed scene context, `Scene::OnRuntimeStop` releases runtime physics, and only then invokes `OnDestroy` and destroys live instances before the runtime scene is replaced. Consequently, managed `OnDestroy` currently cannot resolve entity/component internal calls or access the active physics world. Treat a different ordering as a lifecycle change requiring explicit tests.

The ownership split is deliberate:

- `ScriptEngine` owns one native `ScriptInstance` per running entity.
- Managed `ScriptGlue` owns the actual C# object in an entity-ID keyed registry.
- The native handle stores the assembly pointer, raw 64-bit entity ID, and class index.
- The scene owns `ScriptComponent`; editor field values live in `ScriptEngine::m_FieldStorage`, keyed by stable UUID.

## ABI contracts

Keep these synchronized:

- Export names in `[UnmanagedCallersOnly(EntryPoint = ...)]`, the function-pointer lookup strings in `Assembly`, typedefs in `ManagedFunctions.h`, and call sites.
- `ScriptFieldType` member order and underlying byte width in C++ and C#.
- Blittable layouts for `Vector2`, `Vector3`, `Vector4`, primitive field types, entity IDs, and method argument/return buffers. Managed vectors use sequential layout; preserve the native assumptions such as a 12-byte `glm::vec3`/managed `Vector3` contract.
- C# internal-call delegate signatures, registration names, and C++ callback signatures.
- Boolean and character widths; do not assume C++ `bool` or `char` matches an arbitrary managed declaration without an explicit existing contract.
- Native strings returned by managed exports: managed allocation must be released through the exported `FreeString` path.

`ScriptFieldValue` stores up to 16 bytes with 8-byte alignment. `ScriptFieldTypeSize` is the shared native width authority. `ScriptMarshalling` must cover any new field type.

## Adding a managed component API

1. Add or confirm the native scene component.
2. Add the managed wrapper property or method in `Components.cs`.
3. Add the internal-call delegate and invocation in `InternalCalls.cs`.
4. Add the C++ callback in `ScriptGlue.h/.cpp` and publish it under the exact managed name from `ScriptGlue::GetInternalCalls`.
5. Leave `Assembly::RegisterInternalCalls` as the generic table consumer unless the registration mechanism itself changes.
6. Validate scene context, entity lookup, and component presence in the callback. Follow existing safe defaults for reads and no-op writes.
7. Add C# harness behavior only when the call must originate from user code; otherwise direct method invocation through reflected harness methods may suffice.
8. Exercise the public C# wrapper in the managed harness rather than testing only a raw `InternalCalls` method. Test getters and setters independently, plus missing-context behavior when meaningful.

`TransformComponent::Rotation` is authored as XYZ Euler radians and passed to `glm::quat`; do not silently expose degrees or a quaternion in C#. Direct transform setters mutate ECS state only. They do not teleport an active physics body, whose simulated pose can overwrite the component on a later runtime update.

## Adding an exported managed operation

1. Define the managed `[UnmanagedCallersOnly]` method and keep exceptions behind the managed guard.
2. Add the matching typedef and field to `ManagedFunctions`.
3. Resolve it in `Assembly::ResolveManagedFunctions`; scripting is unavailable when required functions cannot bind.
4. Add the guarded `Assembly` facade operation and then expose it through `ScriptEngine`, `ScriptClass`, or `ScriptInstance` as appropriate.
5. Add an ABI/lifecycle regression.

## Field persistence

- Reflected `ScriptField` metadata describes a class field; it does not store an entity value.
- `ScriptEngine`'s `ScriptFieldMap` is the serialized editor value side table.
- Play-scene copy preserves UUIDs, so it intentionally resolves the same stored editor field map without cloning it.
- Entity duplication must copy the source field map to the new UUID.
- Entity destruction must remove its field storage.
- Runtime edits affect the live instance; replay starts again from stored editor values.
- Scene serialization should only write values compatible with the currently reflected field type.

## Build and test details

Use the required order:

```text
cmake --preset windows-debug
cmake --build --preset windows-debug --target EppoEngineTesting
ctest --test-dir build/debug -R "Scripting|ScriptMarshalling" --output-on-failure
```

`CopyBuildScripts` places `EppoScriptCore.dll`, PDB/deps files, and `runtimeconfig.json` beside the executable. `EppoTestingHarness` builds and deploys `EppoTesting.Scripts.dll`. Run via CTest or from the executable output directory; running from the repository root breaks relative runtime discovery.

Use `Scripting` for discovery, invocation, lifecycle, field values, internal calls, managed exceptions, and C# API behavior. Use `ScriptMarshalling` for enum widths and buffer layout. Also run `Scene` for serialization/copy changes and `Physics` for managed physics changes.

Field-map tests that need reflected field metadata require the suite's shared initialized CoreCLR harness; place them in `Scripting`, not a standalone headless Scene test that initializes and tears down the runtime independently.
