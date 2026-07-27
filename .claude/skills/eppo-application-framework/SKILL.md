---
name: eppo-application-framework
description: Develop and diagnose Eppo's application framework across entry-point creation, Application and layer lifecycle, frame ordering, window and GLFW event delivery, input backends and simulated input, device and renderer startup, resize and minimization, ImGui frame integration, writable and resource directory resolution, the deployed EppoRuntime player, and the App harness. Use for changes to Application, Window, Layer, EntryPoint, Input, SimulatedInput, Event, ImGui, platform window/input code, EppoEditor/Source/EppoEditor.cpp, EppoRuntime/Source, or application-level tests; do not trigger for unrelated Core utilities such as UUID or Hash.
---

# Eppo Application Framework

Read [references/architecture.md](references/architecture.md) before changing startup, frame order, events, input, or ImGui integration. Follow ownership from `main` through `CreateApplication`, `Application`, the window/device, layers, and shutdown.

## Workflow

1. Trace the exact lifecycle phase affected: construction, layer attach, event pump, update, ImGui frame, render submission, present, resize, close, or destruction.
2. Preserve the order dependencies between window creation, required Vulkan extensions, device initialization, renderer initialization, ImGui attachment, and user layers.
3. Keep event-driven state and polled input coherent. Update the real and simulated input paths together when adding input behavior.
4. Add deterministic coverage through `Application::StepFrame` and the support harness when the behavior can be observed by frame count or state.
5. Use headless unit tests for isolated core types; use `App` for real window, device, and repeated-frame behavior, and the `Renderer` suite's `SceneRendering` tests for renderer, input, or camera behavior across frames.
6. Run from the executable output directory or through CTest to preserve runtime resource resolution.

## Guardrails

- Maintain the single live `Application` invariant.
- Do not update or present while minimized or after frame acquisition fails.
- Dispatch events through layers in their current stack order and stop once handled.
- Gate gameplay/editor polled input through the established viewport-input mechanism.
- Shut down GPU and ImGui users before destroying the window or device they depend on.
