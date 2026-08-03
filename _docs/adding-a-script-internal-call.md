# Adding a script internal call

Every native↔managed call the C# side makes crosses four coordinated edits. Miss
the registration-table entry and the name resolves to a **null function pointer**
at call time (not a compile or load error), so this is the classic footgun.

Conventions (see the `eppo-scripting-integration` skill): structs are passed by
pointer, the entity UUID is the first argument, and the live scene is resolved
through `ScriptEngine`'s scene context. `InternalCalls.cs` is the sole unsafe hub
on the managed side.

## The four edits

1. **`EppoEngine/Source/Scripting/ScriptGlue.cpp` — the C function.** Write the
   native function in the anonymous namespace (e.g. `TComponent_GetX(uint64_t id,
   glm::vec3* out)`). Resolve the entity, guard `HasComponent`/validity, and on
   the error path `Log::` rather than silently returning.

2. **`ScriptGlue.cpp` — the registration table.** Add
   `{ "TComponent_GetX", reinterpret_cast<void*>(&TComponent_GetX) }` to the map
   at the bottom of the file. The **string** here is the lookup key C# uses — it
   must match step 3 exactly.

3. **`EppoScriptCore/Source/Core/InternalCalls.cs` — the shim.** Add the
   `delegate* unmanaged[Cdecl]<…>` call that does `Get("TComponent_GetX")` with
   the identical name. This is the only file allowed to hold unsafe glue.

4. **The friendly wrapper** — expose it through the appropriate managed API:
   `Components.cs` for component accessors, `Input.cs`, `Log.cs`, `Physics`, etc.
   User scripts call this, never `InternalCalls` directly.

## Build note

After editing any C#, rebuild the `EppoEngineTesting` / `EppoEditor` target so
the generated Visual Studio dependency or Ninja custom rule recompiles the
managed project and the post-build commands copy its DLLs beside the executable.

## Verify

Run `ctest --test-dir build/bin/Debug-<system>-x86_64 -R Scripting` and the same
command with `-R ScriptMarshalling`. If you added a struct-marshalled
value, the marshalling suite is where a wrong size/layout shows up.
