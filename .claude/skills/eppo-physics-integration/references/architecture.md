# Physics integration architecture

## Responsibility split

`PhysicsWorld` wraps Box3D and owns the Box3D world plus the entity-UUID-to-body map. It creates bodies, attaches already-described colliders, steps simulation, and exposes safe body operations.

`Scene` owns ECS interpretation:

- compose hierarchy transforms;
- find rigid-body roots;
- gather descendant collider components;
- convert authored transforms into rigid-body-local collider data;
- create bodies at runtime start;
- synchronize Box3D world poses back to ECS local transforms after each step.

Keep this split so collider derivation remains reusable outside the editor and Box3D details do not leak across scene code.

## Data model

`RigidBodyComponent` defines static, kinematic, or dynamic body type plus gravity scale and damping. Collider components define authored dimensions, local offset, density, friction, and restitution:

- box: half-size;
- sphere: radius;
- capsule: radius and cylindrical height;
- cylinder: radius and height.

`ColliderData` is the scene-to-physics normalized description. It includes shape type, transformed dimensions/offset, rotation, and material properties. `PhysicsWorld::AttachCollider` maps it to the appropriate Box3D shape definition.

One `RigidBodyComponent` produces one Box3D body, even when it has no colliders.

## Collider gathering

At runtime start, for each rigid-body entity:

1. Compose the entity's world transform and decompose translation, rotation, and scale.
2. Build the Box3D body pose from translation and rotation only.
3. Traverse the rigid-body entity and descendants.
4. Stop traversal when reaching a descendant with its own `RigidBodyComponent`; that node starts a new body boundary.
5. For each collider, compute its pose relative to the root body and apply the composed hierarchy scale to shape dimensions and offsets.
6. Track visited UUIDs to prevent malformed hierarchy cycles from recursing forever.
7. Attach every gathered shape to the root body.

Scale is authored ECS geometry, not part of the Box3D body pose. Mirrored scale must mirror offsets while dimensions remain physically valid magnitudes. Nested rotations rotate collider offsets and local axes into body space for shear-free transforms. Current decomposition approximates rotation when non-uniform scale and rotation compose into shear; do not claim exact collider poses for that case without redesigning the transform representation and adding focused tests.

## Simulation synchronization

`Scene::OnUpdateRuntime` steps physics before scripts. It collects bodies with hierarchy depth, sorts parents before children, and reads each Box3D world pose.

For a root entity, write simulated translation/rotation directly while preserving authored scale. For a parented body, multiply the world pose by the inverse parent world transform, decompose it, and write the resulting local translation/rotation. Parent-first order ensures the inverse uses the current frame's parent pose.

Scripts then observe current transforms and can query or mutate body velocity/impulses through the active physics world.

## Degenerate and missing data

- A body with no collider still simulates; the scene records a warning name for editor display.
- Missing entity/body operations return safe defaults or no-op.
- An expired scripting physics-world weak reference makes managed callbacks safe no-ops.
- Zero/near-zero collider dimensions are clamped or converted according to existing shape behavior; preserve tests such as zero-height capsule and zero-extent box.
- A collider without any rigid-body ancestor creates no body.

## Collider fitting

`Scene::FitColliderToMesh` reads reusable mesh primitive bounds through the active project asset manager and derives authored collider dimensions:

- box from bounds half-extents;
- sphere from the largest relevant extent;
- capsule/cylinder from vertical extent plus radial horizontal extent.

Keep fitting in `Scene`, not `PropertyPanel`, so a runtime or future standalone tool can reuse it. The property panel only triggers the operation.

## Cross-system checklist

When adding or changing a physics property or shape, inspect:

1. `Scene/Components.h` schema/defaults.
2. `Physics/PhysicsTypes.h` and `PhysicsWorld` Box3D mapping.
3. Scene collider gathering, scale, offsets, and runtime sync.
4. Scene copy/duplicate and JSON serialization.
5. `PropertyPanel` editing and fit controls.
6. `SceneRenderer` collider debug meshes/wireframes.
7. C# component API, `Physics` API, internal-call delegates, native callbacks, and registration.
8. Physics and scripting tests.

## Regression matrix

`EppoEngineTesting/Source/Physics/PhysicsWorld.cpp` is intentionally broad. Choose cases from the matrix that match the risk:

| Risk | Cases |
| --- | --- |
| Basic dynamics | gravity, impulse, damping, kinematic velocity, static body |
| Shape mapping | sphere, capsule, cylinder, rotated box, material/dimension behavior |
| Degenerate geometry | zero capsule height, zero box extent |
| Authored scale | scaled root, nested scale, mirrored scale |
| Hierarchy | child colliders, multiple children, nested rotation, nested rigid-body boundary |
| Pose sync | parented dynamic root, child body under moving parent |
| Persistence/copy | serialized physics components, copied scene collider gathering |
| Asset-derived shape | fit every collider type to primitive mesh bounds |

Also run `Scripting` when a managed property or physics call changes and `Scenario` when debug rendering or frame-level behavior changes.

Use `PhysicsWorld::HasBody`, `GetShapeCount`, and `GetPosition` to observe body boundaries, gathered shapes, and authored-to-world pose mapping without reaching into Box3D internals.
