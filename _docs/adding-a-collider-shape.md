# Adding a collider shape

A collider shape is a physics change layered on top of a full component change.
The engine funnels every collider component into a shape-agnostic `ColliderData`,
so the physics core barely changes — but the shape is still a new component and
inherits the whole [component checklist](adding-a-component.md). Read the
`eppo-physics-integration` skill first.

## Physics core (`EppoEngine/Source/Physics`)

1. **`PhysicsWorld.h`** — add the value to `enum class ColliderShape`. If the
   shape needs a dimension the `ColliderData` struct doesn't carry yet (it has
   `HalfExtents`, `Radius`, `Height`), add the field there.

2. **`PhysicsWorld.cpp` — `AttachCollider`** — add the `case ColliderShape::Xxx`
   that builds the Box3D shape and attaches it to the body. `CreateBody`'s
   signature does not change — it just iterates `ColliderData`.

## Scene wiring (`EppoEngine/Source/Scene/Scene.cpp`)

3. **`AppendColliders`** — translate the new collider *component* into a
   `ColliderData{ .Shape = ColliderShape::Xxx, … }`. This is what feeds
   `GatherColliders` and the compound body. Remember the two-walk-direction
   gotcha in CLAUDE.md — parenting/relationship repair affects which colliders get
   gathered.

4. **`Scene::FitColliderToMesh`** — add the overload for the new collider
   component so "fit to mesh" in the inspector works.

## The collider component itself

5. Follow **every** step in [adding-a-component.md](adding-a-component.md) for the
   new `XxxColliderComponent` — struct, both copy lists, **both** serialization
   paths (JSON + packed binary bit + `static_assert`), property panel add/draw,
   and the scripting bridge if scripts touch it. The existing
   `BoxColliderComponent` is the reference; grep it across the tree to see all
   sites at once.

## Debug rendering

6. If colliders are drawn as wireframes, extend the collider wireframe pass in the
   `SceneRenderer` / editor render path so the new shape is visible.

## Verify

Build + `ctest -R Physics` and `-R Scene`. In the editor: attach the collider,
enter play, and confirm the body simulates; save/reload and duplicate to confirm
the component round-trips through both serialization paths.
