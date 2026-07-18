---
name: eppo-rendering-pipeline
description: Develop and diagnose Eppo's Vulkan and NVRHI renderer, including device and swapchain lifecycle, shader compilation and reflection, binding layouts, bindless descriptors, GPU resources, pipelines, render passes, command buffers, SceneRenderer passes, ImGui rendering, and graphical tests. Use for changes under EppoEngine/Source/Renderer, EppoEngine/Source/Platform/Vulkan, renderer-facing ImGui code, editor shaders, or Renderer and Scenario tests.
---

# Eppo Rendering Pipeline

Read [references/architecture.md](references/architecture.md) before changing renderer initialization, bindings, pass construction, or frame submission. Follow a resource from creation through ownership, descriptor registration, binding, command recording, submission, and release.

## Workflow

1. Identify the layer that owns the change: Vulkan platform setup, NVRHI abstraction, reusable GPU resource, shader/reflection contract, render pass, scene submission, or editor presentation.
2. Trace initialization and frame order before editing. Respect the publication order between `DeviceManager`, `Renderer`, the descriptor manager, shader loading, swapchain images, and ImGui.
3. For shader changes, update source, reflected resource expectations, C++ set/binding declarations, pipeline layouts, pass inputs, and tests together.
4. For resources, define lifetime and resize behavior. Preserve bindless handle move-only ownership and avoid retaining stale framebuffer or descriptor handles.
5. Add the smallest renderer regression. Use non-graphical construction tests only where no live device is required; otherwise use the `Renderer` graphical suite or `Scenario` for end-to-end scene rendering.
6. Build before running graphical tests. Run from the executable output directory or through CTest so shaders and resources resolve correctly.

## Guardrails

- Treat descriptor set ordering as an ABI: NVRHI legacy mode maps set numbers to layout-vector indices.
- Keep resource and sampler bindless heaps independent.
- Do not assume swapchain image count equals frames in flight.
- Skip zero-sized viewport resize work and tolerate minimized windows.
- Keep Vulkan-specific code below the renderer abstraction unless the API genuinely cannot express it.
