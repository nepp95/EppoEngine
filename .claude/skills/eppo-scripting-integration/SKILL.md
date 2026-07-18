---
name: eppo-scripting-integration
description: Develop and diagnose Eppo's C++/C# scripting integration across CoreCLR hosting, managed assembly discovery, native internal calls, field and method marshalling, entity script lifecycle, managed build deployment, and scripting tests. Use for changes under EppoEngine/Source/Scripting, EppoScriptCore, script-aware scene/editor code, CMake/Dotnet.cmake, or the Scripting and ScriptMarshalling suites.
---

# Eppo Scripting Integration

Read [references/architecture.md](references/architecture.md) before changing the scripting boundary. Treat the native declarations, managed exports, internal-call registration, serialized field storage, and managed test harness as one contract.

## Workflow

1. Trace the request through every affected layer: C# public API, managed `ScriptGlue`, native `ManagedFunctions` or `ScriptGlue`, `Assembly`, `ScriptEngine`, scene/editor lifecycle, and deployment.
2. Define the ABI before editing. Keep type widths, enum ordinals, calling conventions, entry-point names, argument order, ownership, and string allocation/freeing identical on both sides.
3. Add or update the smallest regression in `EppoEngineTesting/Source/Scripting/`. Extend `EppoEngineTesting/TestData/Scripts/Source/HarnessScript.cs` when managed user code is required.
4. Implement both sides of a cross-boundary change in the same change set. Preserve guarded behavior when the runtime, scene context, entity, component, or physics world is unavailable.
5. Rebuild `EppoEngineTesting` after any C# edit so CMake rebuilds and deploys both managed assemblies.
6. Run `Scripting` and `ScriptMarshalling`; run the broader headless set when lifecycle, scene, physics, or build wiring changes.

## Guardrails

- Initialize CoreCLR once per process; do not design tests around repeated runtime initialization.
- Keep editor field storage authoritative. Push it into new managed instances at runtime start; do not serialize transient managed values.
- Keep `ScriptEngine`'s entity-instance registry authoritative for live script existence.
- Clear scene and physics contexts before their native objects can expire.
- Route reusable runtime APIs through `EppoEngine` and `EppoScriptCore`, not the editor.
