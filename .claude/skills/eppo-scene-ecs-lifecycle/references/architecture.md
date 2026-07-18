# Scene and ECS architecture

## Core model

`Scene` is both an `Asset` and the owner of an `entt::registry`. It maintains an `m_EntityMap` from stable `UUID` to transient EnTT handle. `Entity` is a lightweight pair of handle and raw `Scene*`; it does not own either.

Every entity receives:

- `IDComponent` with stable UUID;
- `TagComponent` with display name;
- `TransformComponent` with authored local translation, Euler rotation, and scale.

Other components are optional. `RelationshipComponent` is intentionally sparse and stores parent/children as UUIDs, not registry handles.

## File map

| File | Responsibility |
| --- | --- |
| `Scene/Components.h` | Native component schemas and defaults. |
| `Scene/Entity.*` | Type-safe component access over an EnTT handle. |
| `Scene/Scene.*` | Entity lifecycle, hierarchy, transforms, runtime coordination, copying, and render submission. |
| `Scene/SceneSerializer.*` | JSON persistence, field persistence, deterministic ordering, and relationship repair. |
| `Renderer/Camera/SceneCamera.*` | Projection state stored by `CameraComponent`. |
| `EppoEngineTesting/Source/Scene` | ECS, hierarchy, copy/duplicate, and malformed-serialization regressions. |

## Identity rules

- EnTT handles are valid only within one registry lifetime.
- `Entity` equality includes both handle and scene pointer.
- UUIDs survive serialization and `Scene::Copy` and are the only supported cross-scene identity.
- `m_EntityMap` must be updated during create, deserialize, copy, and destroy.
- Asset handles identify referenced assets such as meshes or skyboxes; they are separate from entity UUIDs.

Capture UUID values before releasing a scene. Never dereference or inspect an `Entity` after its scene is destroyed.

## Hierarchy and transforms

`RelationshipComponent` stores `Parent` and `Children`. Roots normally have no relationship component. `Scene::SetParent`:

1. Rejects self-parenting and cycles.
2. Captures the child's current world transform.
3. Removes the child from its previous parent's child list.
4. Adds/removes sparse relationship components as needed.
5. Recomputes the child's local transform beneath the new parent so the world pose stays fixed.

`GetWorldTransform` composes local transforms up the UUID parent chain. Protect new traversal code against missing parents and cycles; malformed serialized relationships are repaired, but runtime code should not hang if invariants are temporarily broken.

Deletion of a subtree must detach its root from the external parent, recursively destroy descendants, remove entity-map entries and script field maps, and tolerate deletion during full-scene enumeration.

## Serialization model

`SceneSerializer` writes scene environment and entities. Entities are sorted by UUID to produce deterministic output. Component data is stored explicitly rather than by raw memory layout.

Deserialization creates entities by serialized UUID, populates components, restores script field values when scripting metadata is available, and then repairs relationships. Repair handles:

- a parent that does not list the child;
- a child list that names an entity with a different parent;
- missing parent UUIDs;
- invalid parent while valid children remain;
- duplicate children;
- collider nodes detached by missing ancestry.

Repair preserves world transforms when detaching. Notices are collected for the editor to display after load.

When adding a component, update serialization and deserialization together. Use optional-key handling for backward compatibility when older scenes legitimately lack new properties. Defaults should produce sensible behavior.

## Copy and duplication

`Scene::Copy` creates a new registry and maps each source UUID to a new EnTT handle with the same UUID. Component-copy helpers copy supported component types and environment state. Script field storage remains in `ScriptEngine` under the preserved UUID, so the runtime copy reuses the authored values without copying the side table. Entity handles must never be copied directly.

`DuplicateEntity` creates new UUIDs for the source subtree, copies copyable components and independent script field maps, recreates relationships among the duplicate nodes, and attaches the new subtree consistently. `ScriptFieldType::Entity` values are currently copied as raw UUIDs, so references still point at the original entity even when the target is inside the duplicated subtree. Decide and test whether a feature should preserve or remap those references before changing duplication semantics.

Current recursive duplication has no visited set. It assumes a valid acyclic relationship tree; harden it before relying on duplication of malformed runtime data. Add new component types to both full-scene copy and duplicate paths.

## Runtime lifecycle

Runtime state belongs to the copied play scene:

### Start

- Warn if there is no primary camera.
- Create a `PhysicsWorld` and bodies/colliders from authored components.
- Publish the active physics world to scripting.
- Create and invoke scripts for entities with `ScriptComponent`.

### Update

- Step physics.
- Synchronize body poses into transforms, parents before children.
- Invoke script updates after physics.

### Stop

- Release physics and warnings.
- Invoke script destruction and clear instance state/context through the editor/script lifecycle.

Keep start and stop symmetric when introducing runtime-only systems.

## Rendering boundary

`OnRenderEditor` uses `EditorCamera`. `OnRenderRuntime` resolves the primary `CameraComponent`; it does nothing without one. Both call `RenderScene`, which:

- submits mesh instances using composed world transforms;
- submits point lights in world space;
- submits environment settings;
- lets `SceneRenderer` own GPU details.

Do not place NVRHI/Vulkan command logic in `Scene`.

## Adding a component

Check every applicable surface:

1. Native schema/default/copy semantics in `Components.h`.
2. Scene copy and subtree duplication.
3. JSON serialize/deserialize and backward-compatible defaults.
4. Property panel authoring and add/remove UI.
5. Runtime initialization/update/cleanup.
6. Render submission or physics integration.
7. C# wrapper, internal calls, and native registration.
8. Core umbrella header if it is a public engine type.
9. Focused suite tests plus serialization round-trip coverage.

## Test routing

- `Scene`: entity APIs, sparse relationships, repair, deletion during iteration, and duplication.
- `Physics`: runtime bodies, hierarchy/scale transforms, collider serialization, and copied-scene behavior.
- `Scripting`: component wrappers and script field storage, including duplicate field-map independence under the suite's shared CoreCLR harness.
- `Scenario`: camera movement, loaded scene behavior, and scene-to-renderer integration.
