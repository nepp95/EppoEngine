# Adding a component

A component is a plain `struct` in the ECS registry. The struct is one line of
work; the cost is that every system that walks components must learn about it,
and on this branch that includes **two** serialization paths (JSON + packed
binary). Nothing here is compiler-enforced — a missing entry silently drops the
component from copy, save, packing, or the inspector. Grep the named symbols.

## Engine (`EppoEngine/Source/Scene`)

1. **`Components.h`** — define the `struct`. Keep a default constructor and a copy
   constructor (EnTT copies components); match the surrounding style.

2. **`Scene.cpp` — copy paths (two lists, keep in sync):**
   - `TryCopyComponent<T>` (entity duplication).
   - `CopyComponent<T>` (whole-scene copy, e.g. the play-mode snapshot).
   Miss one and the component vanishes on duplicate or on entering play.

3. **`SceneSerializer.cpp` — JSON path:** add a write block in `SerializeEntity`
   and a matching read block in `Deserialize(path)`. JSON key = struct name.

4. **`SceneSerializer.cpp` — binary/packed path (easy to forget):**
   - Add a `TComponentBit` constant and bump `KnownComponentBits` to cover it
     (the reader rejects a mask with unknown bits set).
   - Add the `componentMask |= entity.HasComponent<T>() ? TComponentBit : 0;`
     line and a write block in `Serialize(BufferWriter&)`.
   - Add the matching read block in `Deserialize(BufferReader&)`.
   - If you write the struct whole via `WriteRaw`/`ReadRaw`, add a `static_assert`
     on its `sizeof`/layout alongside the others at the top of `Serialize` — these
     guard against a field being added without the packed format being updated.

5. **Runtime systems (only if the component drives behavior)** — wire it into the
   relevant `m_Registry.view<T>()` loop in `OnRuntimeStart`/`OnUpdateRuntime`
   (physics, cameras, scripts, render submission). Pure data components skip this.

## Editor (`EppoEditor/Source/Panels/PropertyPanel.cpp`)

6. **`DrawAddComponentEntry<T>("Label")`** — add to the `AddComponent` popup.
7. **`DrawComponent<T>(entity, [](auto& c){ … })`** — add the inspector UI block.

## Scripting (only if scripts read/write the component)

Skip entirely for editor-only data. Otherwise each of these is required — a
missing table entry resolves to a null function pointer at call time. See
[adding-a-script-internal-call.md](adding-a-script-internal-call.md) for the
mechanics; per component you add:

8. **`ScriptGlue.cpp`** — cases in `Entity_HasComponent`, `Entity_AddComponent`,
   `Entity_RemoveComponent`; the `TComponent_GetX`/`SetX` accessors; and an entry
   per accessor in the registration table at the bottom of the file.
9. **`InternalCalls.cs`** — the `delegate* unmanaged[Cdecl]` shim per accessor.
10. **`Components.cs`** — the managed `class TComponent : Component`.

## Verify

Build + `ctest -R Scene` (add `-R Scripting`/`-R ScriptMarshalling` if you touched
the bridge). In the editor: add the component, **save and reload**, and
**duplicate** the entity — confirm values survive both. If it packs, export the
project and confirm the runtime scene still has it (this exercises the binary
path, which the editor never does).
