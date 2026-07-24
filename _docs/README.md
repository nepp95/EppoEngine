# docs

Short "what to touch" checklists for changes where one new *type* ripples across
many files. These are maps, not tutorials — they list the touchpoints so nothing
is silently dropped. None of these lists is compiler-enforced: a missing entry
usually means the type quietly vanishes from copy, save, packing, or the
inspector rather than failing to build.

File paths are stable; line numbers are not — grep for the anchor symbols named
in each doc. When a change spans a subsystem, read the matching `.claude/skills/`
skill too.

- [Adding an asset type](adding-an-asset-type.md)
- [Adding a component](adding-a-component.md)
- [Adding a script internal call](adding-a-script-internal-call.md)
- [Adding an event type](adding-an-event-type.md)
- [Adding a collider shape](adding-a-collider-shape.md)

## Note on the two serialization paths

This branch serializes scenes **twice**: the editor's JSON `.epscene` and the
binary payload packed into `Game.eppak` for the standalone runtime
(`SceneSerializer` has both a `path` and a `BufferReader/Writer` overload). Any
change to per-entity or per-scene data must be made in **both** paths, or scenes
load in the editor but come back wrong (or fail the magic/version check) in an
exported game. This is the single most common thing to forget here.
