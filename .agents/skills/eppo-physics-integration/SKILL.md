---
name: eppo-physics-integration
description: Develop and diagnose Eppo's Box3D integration across rigid bodies, collider shapes, hierarchy-aware collider gathering, world/local transform and scale conversion, runtime simulation and scene synchronization, collider fitting and debug rendering, managed physics APIs, and physics regressions. Use for changes under EppoEngine/Source/Physics, physics-related scene components or runtime code, physics ScriptGlue APIs, property-panel collider editing, or Physics tests.
---

# Eppo Physics Integration

Read [references/architecture.md](references/architecture.md) before changing collider dimensions, hierarchy traversal, pose conversion, or physics scripting. Most physics regressions are transform-contract regressions rather than Box3D API mistakes.

## Workflow

1. State the coordinate space for every pose, offset, rotation, and dimension involved: authored local, composed world, rigid-body local, or Box3D world.
2. Add a focused regression in `EppoEngineTesting/Source/Physics/PhysicsWorld.cpp`; cover hierarchy, rotation, non-uniform or mirrored scale, and degenerate dimensions when relevant.
3. Keep `PhysicsWorld` responsible for Box3D handles and operations. Keep scene traversal, collider aggregation, and ECS synchronization in `Scene`.
4. Update editor component controls, serialization, debug rendering, and C# APIs when changing a physics component.
5. Preserve safe no-op/default behavior for missing bodies, expired worlds, invalid entities, and absent runtime contexts.
6. Run `Physics`; also run `Scene`, `Scripting`, or `Scenario` when their boundary changes.

## Guardrails

- Build one Box3D body per `RigidBodyComponent`; gather descendant colliders until another rigid-body boundary.
- Exclude entity scale from the body pose and apply composed scale to collider geometry and offsets.
- Step physics before scripts so scripts observe the current simulated pose.
- Synchronize parent bodies before children and convert world poses back to authored local transforms.
- Keep bodies without colliders valid and report them without suppressing simulation.
