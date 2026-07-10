---
name: rendering
description: Orientation for the Eppo renderer — Vulkan via NVRHI, shaders, pipelines, framebuffers, meshes, and the scene-level SceneRenderer. Use when working on rendering, graphics, Vulkan, shaders, pipelines, render passes, lighting, the skybox, or the viewport image.
---

# Rendering sector

The renderer is split in two layers: **portable renderer classes** written
against NVRHI handles (`EppoEngine/Source/Renderer/`) and **Vulkan-specific
backend** code isolated in `EppoEngine/Source/Platform/Vulkan/`. Touch the
`Renderer/` layer for engine-facing work; drop into `Platform/Vulkan/` only for
device/swapchain/shader-compilation plumbing.

## Map

- **Scene-level drawing** — `Renderer/SceneRenderer.{h,cpp}`. The main entry
  point. `BeginScene`/`Submit*`/`EndScene`; two passes (`GeometryPass`,
  `SkyPass`). Owns per-frame UBOs (`CameraData`, `LightData` — max 32 point
  lights, `EnvironmentData`) and the instance-transform storage buffer. Batches
  submitted meshes into `m_DrawCommands` keyed by mesh UUID. `GetFinalImage()`
  is what the editor viewport samples.
- **Low-level renderer** — `Renderer/Renderer.h`, `RenderPass.h`, `Pipeline.h`,
  `Framebuffer.h`, `Shader.h` / `ShaderLibrary.h`, `Image.h`, `Mesh.h`,
  `Vertex.h`, and the buffer types (`VertexBuffer`, `IndexBuffer`,
  `UniformBuffer`, `StorageBuffer`).
- **Cameras** — `Renderer/Camera/`: `Camera` (base), `SceneCamera` (serialized,
  drives play mode), `EditorCamera` (free-fly, editor-only).
- **Vulkan backend** — `Platform/Vulkan/`: `DeviceManagerVK`, `LogicalDevice`,
  `PhysicalDevice`, `Swapchain`, `VulkanShader`, `Vulkan.h`. `DeviceManager` is
  the portable interface; `DeviceManagerVK` the implementation.
- **Shaders** — GLSL `.vert`/`.frag` in `EppoEditor/Resources/Shaders/`. Compiled
  **at runtime** via DXC, reflected with spirv-cross, cached to
  `Resources/Shaders/Cache/`. UBO/SSBO layouts must match the C++ structs in
  `SceneRenderer.h` exactly (see the `LightData` / `EnvironmentData` layout
  comments there — colors padded to `float4`, `Params.x` = ambient intensity).

## Conventions

- Everything is NVRHI handles (`nvrhi::CommandListHandle`, `SamplerHandle`, …),
  not raw Vulkan. Keep new rendering code backend-agnostic; put Vulkan specifics
  behind `Platform/Vulkan/`.
- CPU struct ↔ shader cbuffer layout is a silent-corruption trap: change one, and
  change the other in the same commit. Watch padding/`alignas`.
- Trailing-return-type style (`auto Foo() -> void`); `Ref<T>` = shared_ptr.

## Build & verify

Renderer changes need the running editor to confirm — a green build says nothing
about pixels. Build and launch via the `/run-eppo-editor` skill, then eyeball the
viewport. Shader edits don't need a rebuild but **clear `Resources/Shaders/Cache/`
next to the exe** if a shader change seems ignored (stale cache).

If shaders fail to compile with "SPIR-V CodeGen not available", the Microsoft
`dxcompiler.dll` is shadowing the Vulkan SDK one — copy the Vulkan SDK's DLL next
to the exe.
