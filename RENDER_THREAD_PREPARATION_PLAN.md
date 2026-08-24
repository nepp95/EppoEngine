# Render-thread preparation: implementation plan

## Purpose

This document describes the implementation that establishes a render-command boundary, corrects the distinction between frames in flight and swapchain images, and removes the current routine GPU synchronization stalls. The first completed phase still executes render commands on the main thread. Its purpose is to make render work enter through one explicit API and execute at one explicit point so that a later change can move the consumer to a dedicated render thread without redesigning every caller again.

The implementation is intentionally limited to rendering. Mesh import, asset loading, shader compilation, and general resource streaming are not migrated to the render queue in this phase.

## Agreed decisions

- `Renderer::Submit(RenderCommand)` is the public static façade for deferred render work.
- `Renderer` owns the `RenderCommandQueue`; a second renderer singleton is not introduced.
- `Application::StepFrame` executes the queued render commands once per successfully acquired frame.
- Render commands execute on the main thread in this phase.
- `DeviceParams::MaxFramesInFlight` defaults to two and values below two are invalid.
- Vulkan does not request a hard-coded three-image swapchain. It supplies the surface minimum in `VkSwapchainCreateInfoKHR::minImageCount`, queries the actual images returned by Vulkan, asserts that at least two were returned, and uses that result.
- Frames in flight and swapchain backbuffers are independent concepts with independent indices.
- Native swapchain setup and synchronization stay platform-specific. The main application continues to see them through `DeviceManager`.
- Routine presentation proceeds without waiting for the entire Vulkan queue to become idle.
- Render submission returns without resolving GPU timer data; results are resolved after the corresponding frame context completes.
- Resize, swapchain recreation, and shutdown are allowed to wait for idle because they destroy or replace resources that may still be referenced by the GPU.
- This phase does not introduce the actual render thread, mutex-protected queues, scene render packets, or copied ImGui draw data.

## Required end state

The normal frame order must be:

```text
Application::StepFrame
    Window::ProcessEvents
    ThreadPool::Flush

    DeviceManager::BeginFrame
        wait only if the selected frame context is still in flight
        acquire the current native swapchain image
        register the native acquire semaphore as a graphics-queue wait

    Layer::OnUpdate
        collect scene state
        prepare CPU render data
        Renderer::Submit(scene render command)
        Renderer::Submit(runtime composite command, when applicable)

    ImGuiLayer::PrepareRender
    Layer::OnUIRender
    ImGuiLayer::Render
        finalize ImGui CPU draw data
        submit main viewport render command
        submit secondary viewport acquire/render/present commands

    Renderer::ExecuteRenderCommands
        execute exactly one detached FIFO batch on the main thread

    DeviceManager::Present
        make the final graphics submission signal the presentation semaphore
        mark the current frame context as in flight
        queue native presentation
        advance the frame-context index

    NVRHI garbage collection
```

The GPU may still be processing frame N when the CPU begins collecting and recording frame N+1. With the default two frame contexts, the CPU returns to frame context zero at frame N+2 and waits only if that context has not completed yet. This is bounded CPU/GPU overlap. A later render thread adds a separate kind of overlap: main-thread update for N+1 running concurrently with CPU render recording for N.

## Vocabulary and indexing rules

Use the following terms consistently in names and comments.

### Frame context

A frame context is one reusable set of CPU/GPU submission state. The configured maximum is `DeviceParams::MaxFramesInFlight`; it defaults to two and must never be lower than two. With the default, the current frame context rotates predictably:

```text
0, 1, 0, 1, ...
```

Frame-indexed state includes:

- NVRHI command lists owned by `RenderCommandBuffer`
- Overall and named GPU timer queries
- Vulkan acquire semaphores
- NVRHI completion event queries
- Future transient upload arenas and per-frame descriptor allocators

### Backbuffer

A backbuffer is an image returned by the native swapchain. Vulkan chooses the actual count, which this engine requires to be at least two. Acquisition chooses the current backbuffer index independently of the frame-context sequence.

Backbuffer-indexed state includes:

- Native swapchain images
- NVRHI image and framebuffer wrappers for those native images
- Per-image presentation semaphores
- Runtime composite passes and cached framebuffer handles

### Invariant

Never size frame-context arrays from `GetBackBufferCount()`, and never select a swapchain framebuffer with `GetCurrentFrameIndex()`.

The count contracts are:

- `DeviceParams::MaxFramesInFlight >= 2`.
- The actual swapchain image count returned by Vulkan is `>= 2`.
- The effective frame-context count is `min(MaxFramesInFlight, actual swapchain image count)` and is therefore also `>= 2`.

## Implementation order

Implement the work in the following increments. Build after every increment that is expected to compile. The tests are intentionally committed first and may keep the branch red until the corresponding production increment is complete.

---

## Increment A: render-command queue semantics

### Files

- `EppoEngine/Source/Renderer/RenderCommandQueue.h`
- `EppoEngine/Source/Renderer/RenderCommandQueue.cpp`
- `EppoEngineTesting/Source/Renderer/RenderCommandQueue.cpp`

### Required behavior

- Commands execute in submission order.
- A command executes at most once.
- `Clear()` destroys pending commands without invoking them.
- Commands submitted during execution enter the next detached batch.

### Localized implementation change

Replace the body of `RenderCommandQueue::Execute`.

Swap `m_CommandQueue` into a local vector before iteration:

```cpp
std::vector<RenderCommand> commands;
commands.swap(m_CommandQueue);

for (auto& command : commands)
    command();
```

After the local batch finishes, `m_CommandQueue` retains any commands submitted during execution for the next call.

`RenderCommandQueue` remains single-threaded in this phase because submission and execution both occur on the main thread. The detached batch defines the producer/consumer handoff that the future render thread will synchronize.

### Failure and exception behavior

If a render command throws, the exception propagates to `Application` or the runtime error boundary. Stack unwinding destroys the remaining commands in the detached local batch, while commands submitted into `m_CommandQueue` during the failing batch remain pending.

---

## Increment B: static renderer submission façade

### Files

- `EppoEngine/Source/Renderer/Renderer.h`
- `EppoEngine/Source/Renderer/Renderer.cpp`

### Header contract

The public declarations are:

```cpp
static auto Submit(RenderCommand command) -> void;
static auto ExecuteRenderCommands() -> void;
```

The `Renderer` instance owns:

```cpp
RenderCommandQueue m_RenderCommandQueue;
```

The queue remains an instance member. `Renderer::Submit` is a static façade that forwards to the renderer owned by `DeviceManager`.

Declare `m_RenderCommandQueue` after the renderer services captured commands may reference. Reverse member-destruction order then destroys pending lambdas before those services, while the NVRHI device is still alive.

### `Renderer::Submit`

In `Renderer.cpp`, resolve the active renderer through this ownership chain:

```text
Application singleton
    -> DeviceManager
        -> owned Renderer
            -> RenderCommandQueue
```

Inside `Renderer::Submit`, insert:

```cpp
const auto& renderer = DeviceManager::Get()->GetRenderer();
EP_ASSERT(renderer != nullptr, "Cannot submit render work before renderer initialization.");
renderer->m_RenderCommandQueue.AddCommand(std::move(command));
```

Render-command submission begins after `DeviceManager::InitRenderer` publishes the renderer instance.

### `Renderer::ExecuteRenderCommands`

Resolve the same active renderer, assert it exists, and call `m_RenderCommandQueue.Execute()`.

`Application::StepFrame` is the only production call site that drains the queue. Tests may call it directly to verify façade semantics.

### Shutdown behavior

If commands remain pending because no valid frame was acquired, `Renderer` destruction destroys those lambdas and releases their captured `Ref` objects without invoking them.

---

## Increment C: Application execution boundary

### File

- `EppoEngine/Source/Core/Application.cpp`

### `Application::StepFrame`

Inside the successful `BeginFrame()` branch, insert this line immediately after the complete ImGui block and immediately before `m_DeviceManager->Present()`:

```cpp
Renderer::ExecuteRenderCommands();
```

The drain exists only inside the successful `BeginFrame()` branch, after update/UI collection and immediately before `Present()`. This placement supplies an acquired backbuffer and current frame index to the batch, and ensures presentation waits on the generated submissions. Minimized and failed-acquisition frames leave the queue pending.

### Commands submitted outside a frame

Commands submitted during layer attachment or another non-frame phase remain pending until the next successfully acquired frame. Renderer shutdown destroys them if no later frame drains them.

---

## Increment D: split SceneRenderer CPU preparation and GPU execution

### Files

- `EppoEngine/Source/Renderer/SceneRenderer.h`
- `EppoEngine/Source/Renderer/SceneRenderer.cpp`

### Lifetime

`SceneRenderer` inherits `std::enable_shared_from_this<SceneRenderer>`. Every known construction site uses `CreateRef<SceneRenderer>`. Preserve that invariant; constructing a `SceneRenderer` directly on the stack and then calling `EndScene()` will make `shared_from_this()` invalid.

### Responsibilities

Use three distinct methods:

```cpp
auto PrepareRenderData() -> void;
auto UploadRenderData() -> void;
auto ExecuteRender() -> void;
```

`PrepareRenderData` inspects the scene, meshes, submeshes, and materials and populates ordinary CPU structures without an open NVRHI command list.

`UploadRenderData` assumes the render command buffer is open. It performs buffer uploads, descriptor/binding updates, and pass baking.

`ExecuteRender` records the complete pass sequence and submits the NVRHI command list.

### `SceneRenderer::EndScene`

Perform this work immediately:

```text
EnsureColliderMeshes
GatherWireframes
PrepareRenderData
```

Then retain the renderer and submit the GPU phase:

```cpp
const Ref<SceneRenderer> sceneRenderer = shared_from_this();
Renderer::Submit(
    [sceneRenderer]() -> void
    {
        sceneRenderer->ExecuteRender();
    }
);
```

The prepared members retain their values from the `Submit` call until the batch drains.

### Exact contents of `PrepareRenderData`

Place the following CPU work here:

1. Reset and calculate shadow cascade matrices and split distances.
2. Calculate SSAO scalar/vector values:
   - radius
   - bias
   - power
   - intensity
   - inverse render size
3. Clear `m_InstanceTransforms`.
4. Iterate `m_DrawCommands` in stable map order.
5. Assign each draw command's `InstanceOffset` from the current flattened transform count.
6. Append that command's transforms to `m_InstanceTransforms`.
7. Clear `m_WireframeTransforms`.
8. Assign wireframe instance offsets and flatten their transforms.
9. Clear `m_DrawData` and `m_MaterialData`.
10. Iterate each draw command, submesh, and primitive.
11. Create `DrawData` entries from the submesh local transform, instance offset, and new material index.
12. Create `MaterialData` entries from the material's bindless texture indices and scalar/vector properties.

Move these GPU/resource operations from `PrepareRenderData` to `UploadRenderData`:

- `UniformBuffer::SetData`
- `StorageBuffer::SetData`
- `RenderPass::Bake`
- `Image::RegisterBindlessIndex`
- Render-pass input rebinding after resize

The last two lines of the current `FillShadowData` also cross into renderer-resource state:

```cpp
m_ShadowDepthData.ShadowMapIndex = ...GetBindlessIndex(...);
m_ShadowDepthData.ShadowSamplerIndex = ...GetBindlessIndex();
```

Move those assignments to `UploadRenderData`. `FillShadowData` calculates only CPU-visible shadow data.

### Exact contents of `UploadRenderData`

With `m_RenderCommandBuffer->GetCommandList()` active:

1. Resolve the shadow map and sampler bindless indices into `m_ShadowDepthData`.
2. Upload `m_ShadowDepthData` to `m_ShadowDepthUB`.
3. Upload `m_SsaoData` to `m_SsaoUB`.
4. Upload camera, lights, and environment structures.
5. Upload `m_InstanceTransforms`.
6. Upload `m_WireframeTransforms`.
7. Upload `m_DrawData`.
8. Upload `m_MaterialData`.
9. Refresh every pass input that may reference a framebuffer attachment recreated by resize.
10. Bake every pass after its inputs are current.
11. Pre-register the geometry output and bloom mip subresources in the bindless table before any pass binds that table.

Use `vector.data()` for uploads so zero-length vectors follow the storage-buffer API's existing zero-byte behavior.

### `ExecuteRender`

The exact order is:

```text
RenderCommandBuffer::Begin
UploadRenderData
ShadowDepthPass
SsaoPass
GeometryPass
SkyPass
BloomPass
TonemapPass
WireframePass
EP_GPU_COLLECT
RenderCommandBuffer::End
RenderCommandBuffer::Submit
```

Place `EP_GPU_COLLECT` after every render pass has ended and before ending the command buffer.

### Resize remains immediate in this phase

`SceneRenderer::Resize` remains synchronous in this phase. The editor creates an ImGui texture reference from `GetFinalImage()` during UI construction; queued framebuffer recreation could replace that handle before the UI renders.

Treat resize as resource-lifecycle work until the future render packet also owns a stable viewport texture reference.

### Future-thread warning

The queued lambda currently reads mutable `SceneRenderer` members and mesh render handles. This is safe only because `Application` drains the queue before the next main-thread frame begins. The actual render-thread phase must snapshot this data into an immutable or double-buffered packet before allowing main-thread frame N+1 to mutate the same renderer.

---

## Increment E: queue runtime compositing

### Files

- `EppoEngine/Source/Renderer/Renderer.h`
- `EppoEngine/Source/Renderer/Renderer.cpp`

### Split the method

The public API used by `RuntimeLayer` remains:

```cpp
auto CompositeToSwapchain(const Ref<Image>& image) -> void;
```

Add this private immediate method:

```cpp
auto ExecuteCompositeToSwapchain(const Ref<Image>& image) -> void;
```

The public method validates and captures the image:

```cpp
EP_ASSERT(image != nullptr, "Cannot composite a null image to the swapchain.");

Submit(
    [this, image]() -> void
    {
        ExecuteCompositeToSwapchain(image);
    }
);
```

Move the existing body that accesses the current backbuffer, creates or refreshes the composite pass, records the three-vertex draw, and submits the command buffer into the private immediate method.

### Ordering

`RuntimeLayer::OnUpdate` produces this queue order:

```text
SceneRenderer::ExecuteRender
Renderer::ExecuteCompositeToSwapchain
ImGui main viewport render, if enabled
```

FIFO submission to the same NVRHI graphics queue provides the dependency between these three commands.

### Backbuffer caches

Size `m_CompositePasses` and `m_CompositeFramebuffers` from the actual swapchain backbuffer count and select entries with `GetCurrentBackBufferIndex()`. Their framebuffer compatibility is tied to the acquired native image.

---

## Increment F: queue ImGui GPU work without moving ImGui CPU state

### Files

- `EppoEngine/Source/ImGui/ImGuiLayer.cpp`
- `EppoEngine/Source/ImGui/ImGuiRenderer.h`
- `EppoEngine/Source/ImGui/ImGuiRenderer.cpp`

### Critical boundary

`ImGui::Render()` finalizes ImGui's CPU draw data. It stays on the main thread and runs before the render-command batch is executed.

Only the NVRHI buffer upload, command recording, submission, and native secondary-viewport presentation are queued.

### Main viewport API split

Add this public collection method:

```cpp
auto SubmitToSwapchain(ImGuiViewport* viewport, Swapchain* swapchain, bool clearSwapchainTarget = true) -> void;
```

Add this private immediate method:

```cpp
auto RenderToSwapchainImmediate(ImGuiViewport* viewport, Swapchain* swapchain, bool clearSwapchainTarget) -> void;
```

The public collection method captures the pointers and submits a lambda. The private immediate method performs the current `GetOrCreateRenderPass` and `Render` calls without submitting another render command.

The queued viewport command calls the immediate rendering method directly. A nested `Renderer::Submit` would enter the next detached batch and delay the viewport by one frame.

### `ImGuiLayer::Render`

Perform these operations synchronously:

```text
ImGui::Render
submit main viewport GPU rendering
ImGui::UpdatePlatformWindows
ImGui::RenderPlatformWindowsDefault
```

The platform renderer callbacks invoked by `RenderPlatformWindowsDefault` must collect secondary-viewport work rather than execute it immediately.

### Shared `ImGuiViewportData` layout

The struct is currently repeated in `ImGuiLayer.cpp` and `ImGuiRenderer.cpp`. Add the same field in both definitions:

```cpp
bool FrameAcquired = false;
```

Both definitions must remain token-equivalent.

### Secondary `Renderer_RenderWindow` callback

Submit one render command that:

1. Calls the viewport swapchain's `BeginFrame()`.
2. Stores the result in `FrameAcquired`.
3. Returns immediately if acquisition failed.
4. Updates the viewport renderer's font texture if required.
5. Calls the immediate viewport rendering method.

The outer callback calls the immediate viewport rendering method directly.

### Secondary `Renderer_SwapBuffers` callback

Submit a following render command that:

1. Checks `FrameAcquired`.
2. Calls the viewport swapchain's `Present()` only if acquisition succeeded.
3. Clears `FrameAcquired` afterward.

FIFO ordering guarantees that acquisition/render executes before presentation for that viewport.

### Same-frame lifetime constraint

Raw ImGui viewport, renderer, and swapchain pointer captures are valid only while commands drain before `Application::StepFrame` returns to platform-window mutation. The next phase copies `ImDrawData` and retains platform viewport rendering state independently of the live ImGui context.

---

## Increment G: DeviceManager frame-context API

### Files

- `EppoEngine/Source/Renderer/DeviceManager.h`
- `EppoEngine/Source/Renderer/DeviceManager.cpp`
- `EppoEngine/Source/Platform/Vulkan/DeviceManagerVK.h`

### `DeviceParams`

`DeviceParams` contains:

```cpp
uint32_t MaxFramesInFlight = 2;
```

Remove:

```cpp
uint32_t SwapchainImageCount = 3;
```

In `DeviceManager::Create`, change only the existing `MaxFramesInFlight` validation to:

```cpp
EP_ASSERT(params.MaxFramesInFlight >= 2, "MaxFramesInFlight must be at least two.");
```

The effective value may be clamped down to the actual swapchain image count. Because both inputs are required to be at least two, clamping cannot produce a one-frame context.

### Abstract getters

Add or implement:

```cpp
[[nodiscard]] virtual auto GetCurrentFrameIndex() const -> uint32_t = 0;
[[nodiscard]] virtual auto GetMaxFramesInFlight() const -> uint32_t = 0;
```

The backbuffer getters remain separate from the frame-context getters.

### Vulkan forwarding

`DeviceManagerVK` forwards:

```cpp
GetCurrentFrameIndex() -> m_Swapchain->GetCurrentFrameIndex()
GetMaxFramesInFlight() -> m_Swapchain->GetMaxFramesInFlight()
```

`GetMaxFramesInFlight()` returns the swapchain's effective count after clamping, which remains at least two.

---

## Increment H: Vulkan frame synchronization

### Files

- `EppoEngine/Source/Platform/Vulkan/Swapchain.h`
- `EppoEngine/Source/Platform/Vulkan/Swapchain.cpp`

### Remove the constructor's unused local

Delete this unused line from `Swapchain::Swapchain`:

```cpp
VkDevice device = dm->GetLogicalDevice()->GetNative();
```

### Separate `CreateSwapchain` from resize orchestration

`CreateSwapchain` owns native swapchain-dependent resource creation and replacement. Initial swapchain and ImGui viewport construction call it directly. Every later replacement goes through `Resize`, which owns the device-idle wait, forwards the requested or current surface extent, and clears the pending-resize state after successful recreation.

Inside the existing `if (m_Swapchain)` branch, delete:

```cpp
VK_CHECK(vkDeviceWaitIdle(device), "Failed to wait for vulkan device");
```

The callers that replace an existing swapchain enter through `Resize`, so `CreateSwapchain` no longer performs resize synchronization itself.

Inside the recreation branch, add this statement immediately after the presentation-semaphore destruction loop:

```cpp
m_PresentSemaphores.clear();
```

Frame synchronization remains independent of native swapchain-image recreation and is updated by the count-dependent block below.

### Creating frame synchronization objects

After the existing `semaphoreInfo` declaration and before the existing image loop, insert:

```cpp
const auto nvrhiDevice = dm->GetDevice();

if (m_FrameSyncData.size() != m_MaxFramesInFlight)
{
    for (const auto& frame : m_FrameSyncData)
        vkDestroySemaphore(device, frame.AcquireSemaphore, nullptr);

    m_FrameSyncData.clear();
    m_FrameSyncData.resize(m_MaxFramesInFlight);

    for (auto& frame : m_FrameSyncData)
    {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.AcquireSemaphore), "Failed to create acquire semaphore!");
        frame.CompletionQuery = nvrhiDevice->createEventQuery();
        EP_ASSERT(frame.CompletionQuery != nullptr, "Failed to create frame completion query.");
        frame.InFlight = false;
    }
}
else
{
    for (auto& frame : m_FrameSyncData)
    {
        if (frame.InFlight)
            nvrhiDevice->resetEventQuery(frame.CompletionQuery);
        frame.InFlight = false;
    }
}

m_CurrentFrameIndex = 0;
m_FrameActive = false;
```

The first branch runs during initial creation and when a later swapchain image count changes the effective frames-in-flight count. The second branch preserves acquire semaphores and event queries across ordinary recreation.

### `BeginFrame` sequence

Make these changes inside the existing `Swapchain::BeginFrame` body:

1. After `EP_PROFILE_FN`, insert:

   ```cpp
   EP_ASSERT(!m_FrameActive, "BeginFrame was called while a swapchain frame is already active.");
   ```

2. After the existing `dm` and `device` locals, insert:

   ```cpp
   nvrhi::vulkan::IDevice* vkNvrhiDevice(dm->GetDevice()->getNativeObject(nvrhi::ObjectTypes::Nvrhi_VK_Device));
   ```

   Delete the obsolete local that indexes `m_AcquireSemaphores` with `m_AcquireIndex`.

3. At the start of the existing acquire-attempt loop, insert:

   ```cpp
   if (m_ResizePending)
       Resize();

   auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
   if (frame.InFlight)
   {
       vkNvrhiDevice->waitEventQuery(frame.CompletionQuery);
       vkNvrhiDevice->resetEventQuery(frame.CompletionQuery);
       frame.InFlight = false;
   }
   ```

4. Replace the current `vkAcquireNextImageKHR` call and the complete result branch immediately following it with:

   ```cpp
   result = vkAcquireNextImageKHR(device, m_Swapchain, UINT64_MAX, frame.AcquireSemaphore, nullptr, &m_SwapchainImageIndex);

   if (result == VK_ERROR_OUT_OF_DATE_KHR)
   {
       m_ResizePending = true;
       continue;
   }

   if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
   {
       Log::Error("Failed to acquire a swapchain image: VkResult {}.", static_cast<int32_t>(result));
       return false;
   }

   if (result == VK_SUBOPTIMAL_KHR)
       m_ResizePending = true;

   vkNvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, frame.AcquireSemaphore, 0);
   m_FrameActive = true;
   return true;
   ```

5. After the acquire-attempt loop, replace the block beginning with the old `m_AcquireIndex` increment and ending with the existing final `return false` with:

   ```cpp
   Log::Error("Failed to acquire a swapchain image after {} attempts.", maxAttempts);
   return false;
   ```

`VK_ERROR_OUT_OF_DATE_KHR` reaches `continue` before queuing a semaphore wait because no image was acquired. The next loop iteration enters `Resize()` through the pending flag and then retries acquisition. `VK_SUBOPTIMAL_KHR` represents a successfully acquired image, so it marks the resize for the next frame, queues the acquire-semaphore wait, activates the frame, and returns immediately. Successful acquisition no longer falls through to a second result check after the loop.

### `Resize` sequence

In `Swapchain.h`, change the declaration to:

```cpp
auto Resize(uint32_t width = 0, uint32_t height = 0) -> void;
```

Zero dimensions select the current framebuffer extent through the existing `CreateSwapchain`/`SelectExtent` path. This gives deferred acquire/present recovery a valid `Resize()` call while preserving explicit dimensions for window and ImGui viewport resize callbacks.

Immediately after the existing `CreateSwapchain(width, height)` call in `Swapchain::Resize`, add:

```cpp
m_ResizePending = false;
```

The parameterless call from `BeginFrame` therefore waits in `Resize`, forwards `(0, 0)` to `CreateSwapchain`, selects the current framebuffer extent, and clears the pending state before acquisition retries.

### `Present` sequence

Make these changes inside the existing `Swapchain::Present` body:

1. After `EP_PROFILE_FN`, insert:

   ```cpp
   EP_ASSERT(m_FrameActive, "Present was called without an active swapchain frame.");
   ```

2. After the existing `vkNvrhiDevice` local, insert:

   ```cpp
   auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
   ```

3. Immediately after the existing `executeCommandLists(nullptr, 0)` call, insert:

   ```cpp
   vkNvrhiDevice->setEventQuery(frame.CompletionQuery, nvrhi::CommandQueue::Graphics);
   frame.InFlight = true;
   ```

4. Immediately after the `vkQueuePresentKHR` result, delete the combined result check, `vkQueueWaitIdle`, `m_FramesInFlight` loop, `m_QueryPool` reuse/allocation, query reset/set, and queue push.

5. In the deleted code's place, insert this result-and-advance block:

   ```cpp
   m_FrameActive = false;
   m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_MaxFramesInFlight;

   if (result == VK_SUCCESS)
       return true;

   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
   {
       m_ResizePending = true;
       return true;
   }

   Log::Error("Failed to present a swapchain image: VkResult {}.", static_cast<int32_t>(result));
   return false;
   ```

The event query is set after NVRHI emits the final graphics submission, so it represents completion of all render commands and the semaphore signal for this frame. There is no routine `vkQueueWaitIdle` and no replacement FIFO.

### Destruction

In `Swapchain::~Swapchain`, replace only the obsolete loop over `m_AcquireSemaphores` with:

```cpp
for (auto& frame : m_FrameSyncData)
{
    vkDestroySemaphore(device, frame.AcquireSemaphore, nullptr);
    frame.CompletionQuery = nullptr;
}
m_FrameSyncData.clear();
```

---

## Increment I: frame-indexed, asynchronous RenderCommandBuffer timing

### Files

- `EppoEngine/Source/Renderer/RenderCommandBuffer.h`
- `EppoEngine/Source/Renderer/RenderCommandBuffer.cpp`
- `EppoEngine/Source/Renderer/SceneRenderer.cpp`

### `RenderCommandBuffer.h`

Rename the existing private declaration from `EnsureBackBufferCapacity(uint32_t backBufferCount)` to `EnsureFrameCapacity(uint32_t frameCount)`.

After `m_NamedTimestamps`, add the state needed to distinguish completed queries from queries that were never submitted:

```cpp
std::vector<bool> m_FrameSubmitted;
std::vector<std::unordered_set<std::string>> m_SubmittedNamedTimerQueries;
```

Immediately after `m_ActiveCommandList`, add:

```cpp
uint32_t m_ActiveFrameIndex = UINT32_MAX;
```

Resolve completed timing directly in `Begin`, where reuse of a synchronized frame slot is known to be safe.

### Constructor and capacity method

In the constructor, change only the existing capacity call:

```cpp
EnsureFrameCapacity(DeviceManager::Get()->GetMaxFramesInFlight());
```

Rename the `EnsureBackBufferCapacity` definition and its parameter to `EnsureFrameCapacity(const uint32_t frameCount)`. Inside that method:

1. Replace the early-return condition with:

   ```cpp
   if (frameCount <= m_CommandLists.size())
       return;
   ```

2. Replace the five existing vector-resize statements with:

   ```cpp
   m_CommandLists.resize(frameCount);
   m_TimerQueries.resize(frameCount);
   m_Timestamps.resize(frameCount);
   m_NamedTimerQueries.resize(frameCount);
   m_NamedTimestamps.resize(frameCount);
   m_FrameSubmitted.resize(frameCount, false);
   m_SubmittedNamedTimerQueries.resize(frameCount);
   ```

3. Replace the existing command-list/query creation loop header with:

   ```cpp
   for (size_t i = previousCount; i < frameCount; i++)
   ```

### `Begin`

Make these localized changes in `RenderCommandBuffer::Begin`:

1. Change the capacity source and frame-index source:

   ```cpp
   EnsureFrameCapacity(dm->GetMaxFramesInFlight());
   const uint32_t frameIndex = dm->GetCurrentFrameIndex();
   ```

2. After the existing range assertion and before assigning `m_ActiveCommandList`, insert:

   ```cpp
   EP_ASSERT(m_ActiveFrameIndex == UINT32_MAX);
   m_ActiveFrameIndex = frameIndex;

   if (m_FrameSubmitted.at(frameIndex))
   {
       const auto device = dm->GetDevice();
       m_Timestamps.at(frameIndex) = device->getTimerQueryTime(m_TimerQueries.at(frameIndex));
       device->resetTimerQuery(m_TimerQueries.at(frameIndex));

       for (const auto& timerName : m_SubmittedNamedTimerQueries.at(frameIndex))
       {
           const auto& timerQuery = m_NamedTimerQueries.at(frameIndex).at(timerName);
           m_NamedTimestamps.at(frameIndex)[timerName] = device->getTimerQueryTime(timerQuery);
           device->resetTimerQuery(timerQuery);
       }

       m_SubmittedNamedTimerQueries.at(frameIndex).clear();
       m_FrameSubmitted.at(frameIndex) = false;
   }
   ```

The swapchain's `BeginFrame` has completed the wait for this frame context before render-command execution reaches this point, so the block reads and resets only this slot's previously submitted queries.

### Move timing resolution out of `Submit`

Make these changes in `RenderCommandBuffer::Submit`:

1. Replace the locals and range assertion from `const auto& dm = DeviceManager::Get()` through `EP_ASSERT(frameIndex < m_TimerQueries.size())` with:

   ```cpp
   const auto device = DeviceManager::Get()->GetDevice();
   EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
   const uint32_t frameIndex = m_ActiveFrameIndex;
   EP_ASSERT(frameIndex < m_TimerQueries.size());
   ```

2. Immediately after the existing `device->executeCommandList(m_ActiveCommandList)` statement, insert:

   ```cpp
   m_FrameSubmitted.at(frameIndex) = true;
   ```

3. Delete the block beginning with the assignment to `m_Timestamps.at(frameIndex)` and ending after the loop that resets every entry in `m_NamedTimerQueries.at(frameIndex)`.

4. Immediately after the existing `m_ActiveTimerQuery = nullptr` statement, insert:

   ```cpp
   m_ActiveFrameIndex = UINT32_MAX;
   ```

The readback moves to synchronized slot reuse because NVRHI's Vulkan `getTimerQueryTime` waits for availability. Calling it in `Submit` would serialize the CPU with the frame just submitted.

### Named timer methods

In `BeginTimerQuery`:

1. Replace the locals and range assertion from `const auto& dm = DeviceManager::Get()` through `EP_ASSERT(frameIndex < m_NamedTimerQueries.size())` with:

   ```cpp
   EP_ASSERT(m_ActiveCommandList);
   EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
   const auto device = DeviceManager::Get()->GetDevice();
   const uint32_t frameIndex = m_ActiveFrameIndex;
   EP_ASSERT(frameIndex < m_NamedTimerQueries.size());
   ```

2. After creating or retrieving `timerQuery` and before beginning it, insert:

   ```cpp
   m_SubmittedNamedTimerQueries.at(frameIndex).insert(name);
   ```

In `EndTimerQuery`, replace the locals and range assertion from `const auto& dm = DeviceManager::Get()` through `EP_ASSERT(frameIndex < m_NamedTimerQueries.size())` with:

```cpp
EP_ASSERT(m_ActiveCommandList);
EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
const uint32_t frameIndex = m_ActiveFrameIndex;
EP_ASSERT(frameIndex < m_NamedTimerQueries.size());
```

### Timing UI

In `SceneRenderer::RenderGui`, change only these two existing lines:

```cpp
const uint32_t frameIndex = dm->GetCurrentFrameIndex();
EP_ASSERT(frameIndex < dm->GetMaxFramesInFlight());
```

The displayed data is intentionally delayed. A frame context shows the completed timing from the previous time that slot was used. Profiling UI must never stall current rendering for fresher numbers.

---

## Increment J: shutdown and resource lifetime

### Files

- `EppoEngine/Source/Core/Application.cpp`
- `EppoEngine/Source/Platform/Vulkan/DeviceManagerVK.cpp`

### Application destruction

In `Application::~Application`, immediately after the existing `m_ThreadPool->Shutdown(true)` call and before `m_ImGuiLayer.reset()`, insert:

```cpp
EP_ASSERT(m_DeviceManager->WaitIdle(), "Failed to wait for the rendering device during application shutdown.");
```

This ensures that layer destruction can release scene renderers, ImGui buffers, detached viewport swapchains, and framebuffer resources without the GPU still referencing them.

### Vulkan device-manager shutdown

At the beginning of `DeviceManagerVK::Shutdown`, before `GpuProfiler::Shutdown()`, insert:

```cpp
EP_ASSERT(WaitIdle(), "Failed to wait for the Vulkan device during shutdown.");
```

The application-level wait covers the expected lifecycle. The backend-level wait protects alternate owners and partial initialization/shutdown paths.

---

## Automated test contract

The tests are implemented before production completion and are expected to keep the branch red until the contracts above exist.

### Queue tests

- FIFO execution
- Execute-once semantics
- Clear discards without invoking
- Submission during execution waits for the next detached batch

### Renderer façade test

- `Renderer::Submit` does not execute immediately
- `Renderer::ExecuteRenderCommands` drains the active renderer's queue

### Application boundary test

A layer submits a command during `OnUpdate`. The test verifies that the command observes both update and UI collection as complete. This pins the drain after `OnUIRender` and before `StepFrame` returns.

A second application instance disables ImGui. Its layer submits from `OnUpdate`, and the command must still execute in the same successful frame. This prevents the queue drain from being placed accidentally inside the `if (m_ImGuiLayer)` block.

### SceneRenderer lifetime test

Render one scene frame, release the caller's final `Ref<SceneRenderer>` immediately after `EndScene`, and verify that the queued render command retains the renderer until the application drains the batch. Verify that the renderer is released after execution. This pins both deferred execution and the `shared_from_this` capture required by the first phase.

### Frame/backbuffer contract tests

- Assert `GetBackBufferCount() >= 2`.
- Assert `DeviceParams::MaxFramesInFlight >= 2`.
- Assert `GetMaxFramesInFlight() == min(DeviceParams::MaxFramesInFlight, GetBackBufferCount())`.
- Assert `GetMaxFramesInFlight() >= 2`.
- Both current indices remain inside their respective ranges.
- The frame index rotates modulo the effective frame-context count.
- No test assumes a deterministic Vulkan backbuffer acquisition sequence.

### Pending-capture shutdown test

Submit a command that captures a `Ref`, destroy the application before advancing a frame, and verify that renderer-queue destruction releases the captured object.

### RenderCommandBuffer tests

- Timing storage is sized by frames in flight, not backbuffers.
- Begin/end/submit completes with the current frame index.
- Named timers use the frame index.
- Reusing a frame slot resolves and exposes its previous timing without invalid command-list or query reuse.

### Existing renderer regression

Run the composite cycling/resize scenario for more frames than the actual backbuffer count and verify that cached swapchain framebuffer state survives image cycling and recreation.

## Manual verification matrix

After the code compiles and automated tests pass, verify the following with a real display and Vulkan validation enabled.

| Scenario | Expected result |
|---|---|
| Editor idle for several hundred frames | No validation errors; frame index continues rotating. |
| Heavy scene | CPU can enter later frames while prior GPU work remains queued. |
| Main-window resize drag | Recreation may stall, then rendering resumes with refreshed framebuffer caches. |
| Minimize and restore | No rendering while zero-sized/minimized; valid acquisition resumes afterward. |
| Detached ImGui viewport | Secondary acquire, rendering, and present execute in FIFO order. |
| Close detached viewport | No queued command references destroyed viewport data. |
| Runtime compositing | Scene submission precedes composite submission for every frame. |
| Application close | Device becomes idle before layer and swapchain resources are released. |
| GPU timing panel | Values update with a delay while frame pacing remains unaffected. |

Pay particular attention to validation messages concerning:

- Reusing a binary acquire semaphore before its wait submission
- Reusing a command buffer or command pool while in flight
- Destroying a framebuffer or swapchain image while referenced
- Presenting an image without waiting on the rendering-complete semaphore
- Signalling or waiting on a binary semaphore twice without the opposite operation
- Resetting timer queries that have not completed

## Build and test sequence

Build `EppoEngineTesting` in Debug using the generated Visual Studio solution. Then run focused suites in this order:

```powershell
ctest --test-dir build/bin/Debug-windows-x86_64 --output-on-failure -R Core
ctest --test-dir build/bin/Debug-windows-x86_64 --output-on-failure -R App
ctest --test-dir build/bin/Debug-windows-x86_64 --output-on-failure -R Renderer
```

Run the broader non-graphical set:

```powershell
ctest --test-dir build/bin/Debug-windows-x86_64 --output-on-failure --label-exclude graphical
```

The `App` and `Renderer` suites require a display and GPU. Run them through CTest so the working directory remains `EppoEditor/` and shader resources resolve correctly.

Finally launch the editor from `EppoEditor/`, exercise the manual matrix, and inspect both stdout and `latest.log`.

## Deliberately deferred next phase

The implementation above creates a queue boundary but does not make the render workload safe to run concurrently with main-thread mutation.

Before starting a dedicated render thread, implement:

1. Two render-command batches or an equivalent producer/consumer exchange.
2. A render-thread lifecycle owned by `Application` or `Renderer` with explicit startup, wake, frame completion, and shutdown.
3. Immutable or double-buffered `SceneRenderPacket` data containing camera, lights, environment, draw metadata, transform arrays, and retained GPU resource handles.
4. Copied ImGui draw lists and retained per-viewport state so the render thread never reads a live ImGui context while the main thread begins the next frame.
5. A resource-retirement mechanism keyed to completed frame contexts.
6. Thread-affinity assertions for native window operations, renderer submission, queue execution, and resource creation/destruction.

At that point the main thread can collect frame N+1 while the render thread records/submits frame N, and the GPU can simultaneously execute frame N-1. The Vulkan frame-context synchronization implemented in this plan remains the GPU backpressure mechanism for that phase.
