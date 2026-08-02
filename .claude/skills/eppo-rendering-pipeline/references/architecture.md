# Rendering architecture

## Layer map

| Layer | Primary files | Responsibility |
| --- | --- | --- |
| Application ownership | `Core/Application.*`, `Core/Window.*` | Create the window, device manager, renderer, ImGui layer, and drive frames. |
| API-neutral device | `Renderer/DeviceManager.*` | Select the renderer API, expose NVRHI device/swapchain state, and own `Renderer`. |
| Vulkan backend | `Platform/Vulkan/DeviceManagerVK.*`, `PhysicalDevice.*`, `LogicalDevice.*`, `Swapchain.*`, `Vulkan.h` | Create Vulkan instance/device/surface/swapchain and wrap them with NVRHI. |
| Shader backend | `Renderer/Shader.*`, `ShaderLibrary.*`, `Platform/Vulkan/VulkanShader.*` | Load sources, compile/cache SPIR-V through DXC, reflect resources, and create NVRHI shader/layout handles. |
| Resources | `Image`, `Sampler`, vertex/index/uniform/storage buffers, `Framebuffer` | Own NVRHI resources, upload data, resize, and participate in descriptors. |
| Binding | `DescriptorManager.*`, `RenderPass.*` | Own global bindless tables and pass-local binding sets/push constants. |
| Execution | `Pipeline.*`, `RenderCommandBuffer.*`, `Renderer.*` | Create graphics pipelines, record commands/timers, begin/end passes, and composite a final image to the swapchain. |
| Scene orchestration | `SceneRenderer.*`, editor shader resources | Batch scene submissions and execute geometry, sky, and wireframe passes. |
| UI | `ImGui/ImGuiRenderer.*`, `ImGuiLayer.*` | Render ImGui draw data through the same NVRHI device and bindless infrastructure. |

## Initialization order

1. `Application` creates a GLFW window.
2. `DeviceManager::Create` chooses `DeviceManagerVK`; its constructor gathers GLFW's required instance extensions, creates the Vulkan instance and physical/logical devices, and creates the NVRHI device.
3. `DeviceManagerVK::Init` creates the window surface and swapchain resources.
4. `DeviceManager::InitRenderer` publishes a `Renderer` owned by the device manager. The `Renderer` **constructor** creates the descriptor manager, so its global binding layouts exist before anything else runs; `Renderer::Init` then creates the swapchain composite sampler and composite command buffer.
5. `Application` calls `Renderer::LoadShaders(packedShaders, packedIncludes)` — a separate, explicit step, not part of `Renderer::Init`. It iterates the fixed `s_EngineShaderNames` set (`composite`, `geometry`, `imgui`, `skybox`, `wireframe`). Empty arguments mean compile from `Resources/Shaders`; a non-empty packed set that is missing a name, or has an entry with no sources, is an error rather than a silent disk fallback.
6. ImGui attaches **after** shader loading, because `ImGuiRenderer` grabs `GetShader("imgui")` in its constructor during `ImGuiLayer::OnAttach`. Editor layers may then create images and `SceneRenderer` resources.

Do not move shader/resource construction earlier without rechecking calls to `DeviceManager::Get()` and `GetRenderer()->GetDescriptorManager()`, and do not fold `LoadShaders` back into `Renderer::Init` — the runtime needs to supply packed sources between the two.

## Frame flow

`Application::StepFrame` pumps events and, when not minimized, updates layers and submits UI around the device frame:

1. Acquire/begin the current swapchain frame.
2. Update application layers; the editor asks `Scene` to submit to `SceneRenderer`.
3. Begin ImGui, render layer UIs, and end/record ImGui.
4. Submit recorded command lists.
5. Present the acquired swapchain image.

Keep acquisition failure and zero-size/minimized paths safe. Swapchain resize recreates image/framebuffer state; anything caching those handles must be refreshed.

## SceneRenderer flow

`Scene::RenderScene` visits mesh and point-light components and submits composed world transforms plus environment data. `SceneRenderer` separates collection from execution:

- `BeginScene` selects editor or scene camera data and resets per-frame submission state.
- `SubmitMesh` batches instances by mesh/draw key.
- `SubmitPointLight` and `SubmitEnvironment` fill scene buffers.
- `EndScene` flattens and uploads instance transforms before command recording; `PrepareRender` then uploads camera, light, and environment buffers.
- `GeometryPass` renders material geometry to the main framebuffer.
- `SkyPass` draws the environment.
- `WireframePass` draws debug colliders, selected-entity highlights, and mesh wireframes when enabled.
- `EndScene` records/submits the command buffer and exposes the final image to the editor viewport.

`EditorLayer` calls `SetScene` every frame because edit/play transitions replace the active scene while the renderer object survives.

## Presenting the final image

The editor displays `SceneRenderer`'s final image as an ImGui viewport texture. A deployed runtime has no such panel, so `RuntimeLayer` calls `Renderer::CompositeToSwapchain(image)` instead: a full-screen three-vertex draw through the `composite` shader straight into the current swapchain framebuffer.

Its pass state is **per back buffer and lazily built** — `m_CompositePasses` / `m_CompositeFramebuffers` are sized to the back-buffer count, and an entry is rebuilt when it is empty or when the swapchain handed back a different `nvrhi::FramebufferHandle` (which is what a resize looks like from here). Anything caching a framebuffer handle must follow the same compare-and-rebuild rule.

## Shader and binding contract

Shader sources live in `EppoEditor/Resources/Shaders`, with includes under `Resources/Shaders/Includes`. The editor and graphical tests read them directly from the `EppoEditor/` working directory. Compiled SPIR-V is cached in `FS::GetShaderCacheDirectory()` — `Resources/Shaders/Cache` when no writable directory is configured, otherwise `<writable>/ShaderCache`.

A `ShaderSpecification` carrying `Sources` is packed: it compiles those, and resolves `#include`s only from its `Includes` map through a handler that never touches the filesystem. A deployed runtime ships no shader files, so an include missing from the pack fails the compile rather than finding a stray file on disk. Without `Sources` the shader is compiled from `Resources/Shaders` with DXC's default (disk-reading) include handler. `Renderer::LoadShaders` takes the packed set or nothing, and treats a packed entry that has no sources as missing rather than letting it degrade into a disk compile. A failed compile logs and asserts in the constructor: it means a broken editor build, and `EP_ASSERT` throws under `EP_DIST`, so a deployed game surfaces it through the runtime error dialog.

`VulkanShader` compiles and reflects each stage. Reflection populates:

- vertex input attributes and stride;
- resource bindings grouped by descriptor set;
- push-constant range;
- NVRHI binding layouts ordered by ascending set.

NVRHI legacy Vulkan binding mode treats the layout vector index as the Vulkan descriptor-set number. A missing set in the middle shifts every later set. `Shader::GetBindingLayouts` therefore returns an ordered map, and `RenderPass::Bake` merges static pass bindings with global bindless layouts without gaps.

When changing a shader binding:

1. Update the shader declaration and stage usage.
2. Confirm reflection recognizes its NVRHI resource type and array size.
3. Update pass `SetInput(set, binding, resource)` or bindless registration.
4. Update push-constant declaration if applicable.
5. Confirm pipeline layout order and pass baking.
6. Editing a file under `Resources/Shaders/Includes` invalidates the cache on its own: the cache hash covers the top-level `.vert`/`.frag` source plus every include's contents.
7. Add or update `Shader`, `Pipeline`, or `RenderPass` tests.

## Bindless ownership

`DescriptorManager` owns separate resource and sampler heaps. Each heap has a binding layout, descriptor table, capacity, next sequential slot, free list, and mutex.

`BindlessHandle` is move-only RAII. Destruction or move-assignment releases an owned slot to the originating manager through a weak reference. Preserve these invariants:

- invalid index is `uint32_t` max;
- released slots are preferred before heap growth;
- resource and sampler indices are independent;
- growth cannot exceed the declared maximum table capacity;
- moving a handle transfers ownership exactly once;
- a resource must not outlive the descriptor data it points at unless the descriptor is rewritten or released.

Images, uniform buffers, storage buffers, and samplers register through the appropriate heap. Materials store bindless indices rather than owning the global tables.

## Resource and resize rules

- `Framebuffer` owns its color/depth images and rebuilds them on resize.
- `Pipeline` derives current size from its framebuffer; resize through the owning pass/pipeline path.
- Buffer resize must preserve intended usage flags and rewrite descriptors when the underlying NVRHI handle changes.
- `RenderCommandBuffer` allocates timing data per back buffer, not merely per frame in flight.
- Use NVRHI handles for lifetime management; use raw native Vulkan handles only inside the backend and swapchain bridge.

## Tests

Renderer tests are registered as graphical because most require a live Vulkan/NVRHI device. The suite covers device availability, descriptor allocation/lifetime/growth, pipeline layout order, pass binding-set baking, shader layouts, framebuffer creation, command submission/timers, sampler ownership, and mesh material indices.

Behavior that must traverse `Scene -> SceneRenderer` or editor-camera input belongs in the same suite's `SceneRendering` tests, which drive multiple frames through `TestContext`/`ScenarioLayer`. Run graphical suites only with a real display and GPU. Headless CI excludes the `graphical` label, so report any unexecuted graphical coverage explicitly.
