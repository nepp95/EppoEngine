---
name: eppo-scene-ecs-lifecycle
description: Develop and diagnose Eppo scenes and ECS behavior across EnTT entities, component ownership, UUID identity, parent-child relationships, world transforms, duplication and copying, scene serialization and repair, runtime start/update/stop, render submission, physics and scripting coordination, and scene tests. Use for changes under EppoEngine/Source/Scene or any feature that adds or changes scene components.
---

# Eppo Scene and ECS Lifecycle

Read [references/architecture.md](references/architecture.md) before adding components or changing hierarchy, copy, serialization, or runtime behavior. Treat component definition, copying, persistence, editor exposure, scripting exposure, and tests as one feature surface.

## Workflow

1. Establish the identity and ownership effects: transient EnTT handle, stable UUID, scene pointer, asset handle, relationship UUID, or runtime-side object.
2. Add a regression first in `EppoEngineTesting/Source/Scene/`, or in Physics/Scripting when the behavior crosses those runtime systems.
3. Update every component touchpoint: `Components.h`, scene creation/copy/duplicate logic, serializer read/write, editor property UI, and managed wrappers/internal calls when exposed to scripts.
4. Preserve hierarchy consistency and world transforms through reparenting, repair, duplication, deletion, scene copy, and physics synchronization.
5. Keep runtime start/update/stop symmetric. Create runtime-only state on start and release it on stop without leaking values into the authored scene.
6. Run `Scene` plus every affected integration suite.

## Guardrails

- Use UUIDs across scene copies and serialization; never persist EnTT handles or `Entity` wrappers.
- Keep `m_EntityMap` synchronized with the registry.
- Treat `RelationshipComponent` as sparse: roots need not carry it.
- Iterate the whole scene through `ForEachEntity`; sort by UUID before deterministic serialization.
- Make malformed relationship data recoverable without discarding otherwise valid entities.
