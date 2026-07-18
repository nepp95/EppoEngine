# Plan: RenderCommandBuffer + RenderPass binding-set ownership

**Status:** Revised 2026-07-15 per reviewer feedback (`render-command-buffer-review.md`) + user architecture corrections. Ready for implementation once the user signs off on the revised split below.
**Branch:** `refactor/renderpass-merge` (git worktree at `.worktrees/renderpass-merge`)
**Base commit:** `29d8604` (after "Merge common render-pass boilerplate into RenderPass")
**Out of scope for this PR:** `Renderer::BeginRenderPass/EndRenderPass/RenderMesh` — this PR prepares the ground; the `Renderer::*` API is a follow-up.

### Changes from the original approved plan (driven by reviewer + user)

- **Reviewer Issue 1 (setGraphicsState cadence):** addressed by NOT bundling draws into the RCB. The RCB exposes only `SetGraphicsState(state)` + `GetCommandList()`; call sites retain their existing per-submesh `SetGraphicsState` cadence (SceneRenderer) / per-draw (ImGui). See Step 4 sketch.
- **Reviewer Issue 2 (Step 4 loop nesting ambiguity):** addressed by the explicit 3-level-nested sketch in Step 4 (`drawCmd → submesh → primitive`).
- **User correction — RCB owns lifecycle, not RenderPass.** `Begin/End/Submit`, marker open/close, timer-query begin/end + readback, and `GetTime/GetTimeMs` all move from `RenderPass` to `RenderCommandBuffer`. RenderPass retains only its spec + the new binding-set recipe API.
- **User correction — RCB has no DrawIndexed.** No `Draw`/`DrawIndexed`/`setPushConstants`/draw-recording helpers on the RCB. Recording happens on the command list the RCB exposes via `GetCommandList()`.
- **User correction — SetGraphicsState is single-responsibility.** `rcb.SetGraphicsState(state)` only sets the graphics state on the held command list. No push-constant / draw / clear bundling. No "ApplyState"-style merged API.
- **User correction — RenderPass holds no GraphicsState.** Callers build their own `nvrhi::GraphicsState` local from `pass.GetPipeline()` etc. + `pass.GetBindingSet()` / `pass.GetBindingTable()` after `pass.Bake()`.
- **User correction — PassStatistics lives on the renderer, not on the RCB.** SceneRenderer holds `PassStatistics m_GeometryStats` etc. (one per pass), ImGuiRenderer holds `PassStatistics m_Stats`. Neither the RCB nor RenderPass carries the manual counters. RCB owns only the GPU-timer-derived time (`GetTime/GetTimeMs`).
- **User correction — binding-set registration is "SetInput + Bake", not SetBindingSet(desc, layout).** New `SetInput(set, binding, nvrhi::IResource*)` + `DeclarePushConstants(set, size)` + `SetBindingTable(set, table)` accumulate a per-frame recipe; explicit `Bake()` runs `device->createBindingSet` per registered set using `Shader::GetBindingLayouts()`. Bake uses shader reflection (`Shader::GetShaderResources()` — new one-line accessor) to choose the right typed `BindingSetItem::X(...)`.

---

---

## Goal

Split the responsibilities currently crammed into `SceneRenderer`'s per-pass functions across two cooperating abstractions, and remove the draw-recording boilerplate duplicated across all four call sites (Geometry, Skybox, Wireframe, ImGui).

### Target architecture (forward-compatible)

The end-state we are **heading toward** (next PR, NOT this one):

```cpp
Renderer::BeginRenderPass(rcb, pass);
Renderer::RenderMesh(rcb, pipeline, drawCmd, material);
Renderer::EndRenderPass(rcb);
```

This PR builds the two primitives that `Renderer::*` will eventually orchestrate: **`RenderCommandBuffer`** (owns the recording: command list, debug markers, GPU-timer-derived pass stats, and the lifecycle of Begin/End/Submit) and **`RenderPass`** (owns the pass's data — pipeline/framebuffer + a recipe of registered binding-set inputs that Bake can run per frame — but no longer a lifecycle or a `GraphicsState` of its own). Call sites in this PR still drive the lifecycle directly (`rcb.Begin(name)` / `rcb.End()` / `rcb.Submit()`); the move to a `Renderer::*` facade is deferred.

### Responsibilities after this PR

**`RenderPass` — owns the pass's static data and binding-set recipe. "Manages the inputs of the pass."**
- `RenderPassSpecification` (name, pipeline, clear flags) — already present. (Clears are applied by the caller at the call site — see Ownership below. Only the Geometry pass clears today; ImGuiRenderer already clears its own inline.)
- **NEW: binding-set inputs.** A per-frame recipe accumulator plus a Bake step:
  - `auto SetInput(uint32_t set, uint32_t binding, nvrhi::IResource* resource) -> void` — `set` = binding-layout index (the map key of `Shader::GetBindingLayouts()`; 0 or 1 in current passes), `binding` = the shader slot/register within that set (the single integer passed to `BindingSetItem::X(...)`). Type-disambiguation when multiple resource types share a `(set, binding)` (e.g. Geometry Set-0 has `PushConstants(0)`, `Sampler(0)`, `StructuredBuffer_SRV(0)`) is resolved at Bake time via shader reflection — see "Bake detail" below.
  - `auto DeclarePushConstants(uint32_t set, uint32_t size) -> void` — records a `BindingSetItem::PushConstants(binding, size)` entry. Separate from `SetInput` because push constants are a layout declaration, not a resource handle.
  - `auto SetBindingTable(uint32_t set, nvrhi::DescriptorTableHandle table) -> void` — records the Geometry material-descriptor-table path (Set-1 today).
  - `auto Bake() -> void` — caller invokes after all SetInputs/DeclarePushConstants/SetBindingTable for this frame are done; for each registered set, `device->createBindingSet(desc, shader->GetBindingLayouts().at(set))` is run and the resulting `nvrhi::BindingSetHandle` is stored. No cache, no invalidation — recreated each frame exactly as today.
  - `auto GetBindingSet(uint32_t set) const -> const nvrhi::BindingSetHandle&` and `auto GetBindingTable(uint32_t set) const -> const nvrhi::DescriptorTableHandle&` — caller adds these to its locally-held `nvrhi::GraphicsState.bindings`.
- Existing accessors kept: `GetPipeline()`, `GetSpecification()`, `GetName()`, `Resize()`.
- **Gone from RenderPass:** `m_Statistics` + `GetStats()`, `m_TimerQueries` + `m_Timestamps` + `GetTime/GetTimeMs`, `Begin/End/Submit`, the open-cl + marker + timer + clear + state-population logic in `Begin`. None of those live here anymore.

**Bake detail (type disambiguation):** today's call sites know the type of each BindingSetItem because they call the typed constructor (`BindingSetItem::ConstantBuffer(...)`, `::Sampler(...)`, `::StructuredBuffer_SRV(...)`). `SetInput` deliberately accepts a typed-erased `IResource*`. To rebuild the right typed BindingSetItem at `Bake()` time, RenderPass consults the shader's reflected resource table. `Shader::m_ShaderResources` already exists (`Shader.h:76`) but is `protected`; expose it as `GetShaderResources() const` so RenderPass can do `(set, binding) + IResource type` → reflected `ResourceType` lookup. If two distinct resource types share `(set, binding)` (as PushConstants / Sampler / StructuredBuffer_SRV do at `(0, 0)` in Geometry) they're differentiated by `IResource*`'s dynamic type and the matching reflection entry. This is straightforward in practice — the existing BindingSetItem construction already encodes exactly this knowledge; the recipe just moves it one layer down.

**`RenderCommandBuffer` — owns the recording lifecycle. "Owns the command list + the GPU-timer-derived pass stats + debug markers."**
- Holds: `nvrhi::CommandListHandle`, per-frame `nvrhi::TimerQueryHandle` array + `std::vector<float> m_Timestamps` (moved from `RenderPass`). Holds the **GPU-timer-derived** pass stats — the render time, expressed as `GetTime(frameIndex)` / `GetTimeMs(frameIndex)` (moved from `RenderPass`). Does NOT hold the manually-incremented CPU counters (DrawCalls/Meshes/etc.) — those are caller-owned per-pass members now.
- Does NOT hold a `nvrhi::GraphicsState` member. Call sites hold their own `nvrhi::GraphicsState` local and push it down via `SetGraphicsState(state)`.
- API (every method is single-responsibility; no helper bundles push-constants or draws):
  - `auto SetGraphicsState(const nvrhi::GraphicsState& state) -> void` — sets the graphics state on the held command list (`m_CommandList->setGraphicsState(state)`). THAT IS ALL IT DOES. Push-constant uploads, draws, clears — none of those live here.
  - `[[nodiscard]] auto GetCommandList() const -> const nvrhi::CommandListHandle&` — for the few recording ops `SetGraphicsState` doesn't cover (`setPushConstants`, `drawIndexed`, `draw`, `nvrhi::utils::ClearXxx`). Exposes the held cl directly; the call site is responsible for what it pushes.
  - `auto Begin(const std::string_view name) -> void` — `open()` the cl, `beginTimerQuery` for this frame index, `beginMarker(name)`.
  - `auto End() -> void` — `endMarker()`, `endTimerQuery()`.
  - `auto Submit() -> void` — `close()`, `device->executeCommandList()`, read back the timer into `m_Timestamps[frameIndex]`, `device->resetTimerQuery()`.
  - `[[nodiscard]] auto GetTime(uint32_t frameIndex) const -> float` / `GetTimeMs(frameIndex)` (moved from `RenderPass`).
- RCB is **caller-owned and persistent** (one per pass in SceneRenderer; one per ImGuiRenderer). The future `Renderer::BeginRenderPass(rcb, pass)` takes it by reference; rcb outlives End/Submit so the caller's PassStatistics member (separate) and the RCB's `GetTimeMs` both survive for `RenderGui`.

### Ownership and lifetime

- SceneRenderer holds `RenderPass m_GeometryPass` + `RenderCommandBuffer m_GeometryRCB` + `PassStatistics m_GeometryStats` (likewise Sky/Wireframe). ImGuiRenderer holds `RenderPass m_Pass` + `RenderCommandBuffer m_RCB` + `PassStatistics m_Stats`.
- The `m_Statistics` member and `GetStats()` accessors leave `RenderPass`. **They do NOT migrate to the RCB** — they migrate to the *renderer* holding the pass (SceneRenderer/ImGuiRenderer). The RCB keeps only the GPU-timer-derived time (`GetTime/GetTimeMs`); the manually-incremented counter struct (`DrawCalls++ / Vertices += ...`) lives next to the call site that bumps it.
- Caller drives lifecycle directly: `rcb.Begin(name)` / build & push `nvrhi::GraphicsState` via `rcb.SetGraphicsState(state)` (per-submesh in the scene passes, per-draw in ImGui, once before Sky's single draw) / `rcb.GetCommandList()->setPushConstants / drawIndexed` per primitive / `rcb.End()` / `rcb.Submit()`.
- Pass-global binding sets are registered on `RenderPass` (`SetInput` / `DeclarePushConstants` / `SetBindingTable`) and baked (`Bake()`) at the start of each pass function, then read via `GetBindingSet` / `GetBindingTable` and added to the locally-held `nvrhi::GraphicsState.bindings`. Same per-frame `device->createBindingSet` cost as today, just de-duplicated from 3 call sites into one.
- `RenderGui` reads `m_GeometryStats` etc. (the renderer's per-pass members) instead of `m_GeometryPass.GetStats()`. Subtotal aggregation unchanged. Timer reads `m_GeometryPass.GetTimeMs(frameIndex)` become `m_GeometryRCB.GetTimeMs(frameIndex)` (moved from pass to RCB).
- Framebuffer clears: applied at the single call site that needs them (Geometry), using `rcb.GetCommandList()` + the framebuffer from the pass's pipeline spec. RenderPass exposes enough to know whether clears are wanted (`GetSpecification().ClearColor / .ClearDepth`) but does not own the clear-recording code itself. ImGuiRenderer keeps its existing manual clear inline.

### What stays where (explicit, to avoid scope creep)

- **`nvrhi::GraphicsState` construction (pipeline/framebuffer/viewport/scissor/vertex buffers/index buffer/binding sets)** → caller's local in each `*Pass()` function. RCB does not hold it.
- **`setGraphicsState` push** → `rcb.SetGraphicsState(state)` (single-responsibility wrapper). No DrawIndexed/Draw/PushConstants bundling.
- **Push-constant upload, draw, manual CPU stats bump** → call site (`rcb.GetCommandList()->setPushConstants / drawIndexed; stats++`). This is the same per-primitive pattern as today; nothing wraps it.
- **Per-draw binding-set swap (ImGui)** → stays in ImGuiRenderer (texture cache + per-draw mutation of the local `nvrhi::GraphicsState.bindings` + `rcb.SetGraphicsState`).
- **Pass-global Set-0 creation** → moves into `RenderPass` via `SetInput` + `Bake`.
- **Material descriptor table (Set-1)** → moves into `RenderPass` via `SetBindingTable` + `Bake`/`GetBindingTable`.
- **Timer queries + GetTime/GetTimeMs** → moves from `RenderPass` to `RenderCommandBuffer`.
- **PassStatistics (manual counters)** → moves from `RenderPass` to the *renderer* (one `PassStatistics` member per owned pass). NOT to the RCB.
- **Begin/End/Submit lifecycle, marker open/close, cl open/close/execute, timer begin/end** → moves from `RenderPass` to `RenderCommandBuffer`.

---

## Verification against the codebase (done during planning)

- `EppoEngine/Source/Renderer/RenderPass.{h,cpp}` — current state read; `Begin/End/Submit` open/close the cl + begin/end marker + begin/end timer + apply clears + build & return a `nvrhi::GraphicsState` by value; `m_Statistics` is reset in `Begin`; timer queries (per-frame-in-flight, `m_TimerQueries`) + `m_Timestamps` + `GetTime/GetTimeMs` live on pass. Confirmed: the lifecycle/stuff moves to RCB, the `m_Statistics` member leaves RenderPass for the renderer side, and the by-value GraphicsState return path goes away (callers now build state themselves).
- `EppoEngine/Source/Renderer/SceneRenderer.{h,cpp}` — 3 scene passes, each with verbatim dup of: `createBindingSet(desc, layouts.at(0))` + `state.addBindingSet(...)`, vertex/index/pushConstants/drawIndexed/stats++. Verified at `scene_renderer.cpp:367-382` (Geometry Set-0 + Set-1 descriptor table), `:460-461` (Sky), `:616-617` (Wireframe); per-submesh pattern at `:398-403`/`:629-634`; per-primitive pattern at `:408-429`/`:640-656`. Confirmed: lifetime/RCB removes the per-pass `Begin(m_CommandList)` returning-state pattern, `SetInput`+`Bake` removes the createBindingSet+addBindingSet duplication. Vertex/index/PC/draw/CPU-stat-bump pattern stays where it is (call site, on `rcb.GetCommandList()`), unchanged in shape.
- `EppoEngine/Source/ImGui/ImGuiRenderer.{h,cpp}` — pass owns no pipeline / no Set-0 (constructed `RenderPassSpecification{ .Name = "UI" }`); per-draw `state.bindings = { GetOrCreateBindingSet(texture) }` at `ImGuiRenderer.cpp:192`; vertex/index binding done once outside the per-draw loop (kept); `setGraphicsState` at `ImGuiRenderer.cpp:217` is per-drawCmd because bindings + scissorRect change per-draw. Confirmed: ImGui's pass stays pipeline-less, its per-draw binding swap stays in ImGuiRenderer (now via mutation of the local `nvrhi::GraphicsState.bindings` + `rcb.SetGraphicsState(state)`), and its manual clear stays inline.
- `EppoEngine/Source/Renderer/Pipeline.{h,cpp}` + `Shader.h` — `PipelineSpecification` owns Shader; `Shader::GetBindingLayouts()` returns `unordered_map<uint32_t, BindingLayoutHandle>` (set keys); `Shader::GetDescriptorTable()` returns the material table; `Shader::m_ShaderResources` (`Shader.h:76`) holds the per-set reflection table needed for `SetInput`→`Bake` type disambiguation — currently `protected`, will need a `GetShaderResources()` accessor (a one-line addition).
- `EppoEngine/CMakeLists.txt` — `file(GLOB_RECURSE SOURCES ...)`. New `RenderCommandBuffer.{h,cpp}` is picked up **automatically**; no CMake edit.
- `EppoEngineTesting/CMakeLists.txt` — `AddTestingSuite(Renderer graphical)` already registered. `RendererTests.cpp` already exists and is GLOBBED. No CMake edit for new tests.
- `EppoEngine/Source/EppoEngine.h` — public umbrella header; check whether `RenderPass.h` is included and add `RenderCommandBuffer.h` if so (see Step 0).
- No `Renderer::*` API currently exists (`Renderer.h` is just a ShaderLibrary holder). This PR does NOT touch it.

---

## Test plan (TDD — tests first, then implementation)

All tests go in `EppoEngineTesting/Source/Renderer/RendererTests.cpp` (graphical suite — already registered). Each test early-returns when `AppHarness::IsAvailable()` is false (headless). All use the existing `MakeTestPipeline()` helper for pipeline construction.

The four existing `RenderPass_*` tests are migrated to the new design (their assertions are recast; the suite name is unchanged). `Pipeline_CustomBlendState_*` is untouched. New tests pin the new responsibilities.

### Tests to write first (failing initially, then made green)

1. **`RenderCommandBuffer_BeginEndSubmitLifecycle`** — `rcb.Begin("X")` opens the held command list and writes the marker + begins the timer query; `rcb.End()` ends the marker + timer query; `rcb.Submit()` closes + executes + reads back the timer into the rcb's `m_Timestamps[frameIndex]`. Multiple frames respect `DeviceManager::GetCurrentBackBufferIndex`. Pins that lifecycle moved off RenderPass.
2. **`RenderCommandBuffer_GetTimeMsAfterSubmit`** — after `Begin/End/Submit`, `rcb.GetTimeMs(frameIndex)` returns a positive value; before Submit it returns the default 0. Pins that GetTime/GetTimeMs moved off RenderPass onto the RCB.
3. **`RenderCommandBuffer_SetGraphicsStateIsSingleResponsibility`** — `rcb.SetGraphicsState(state)` is verifiably a thin wrapper over `m_CommandList->setGraphicsState(state)` — set a state with a distinctive pipeline/framebuffer/viewport, then read it back via the held command list (or assert that the underlying call was recorded). Verifies there are no draw/PC/clear side-effects packed in. (Implementation note: this is a behavioral, not introspectable, assertion in NVRHI; the test guards the API contract — if someone adds a `drawIndexed` call inside SetGraphicsState, this test should fail by counting extra commands on the cl. Pragmatic form: capture the cl into a mock/recording cl if NVRHI exposes one; otherwise assert via a single-frame test run + operation counts that marrying state-set with draw is rejected by the API shape, e.g. attempting `rcb.DrawIndexed(...)` is a compile error.)
4. **`RenderCommandBuffer_HasNoPassStatistics`** — static_assert / compile-only test that `RenderCommandBuffer` does NOT expose any `GetStats()` accessor and does NOT have a `PassStatistics` member. Pins the user's "arbitrary counters don't belong on the RCB" rule.
5. **`RenderCommandBuffer_HasNoDrawIndexed`** — static_assert that `rcb.DrawIndexed(...)` / `rcb.Draw(...)` are not members of the RCB. Pins the user's "do not add a DrawIndexed method to RCB" rule.
6. **`RenderPass_OwnsPipelineAndBindingRecipe`** — pass with owned pipeline; after registering `SetInput(0, 1, cameraUB->GetBuffer())` etc. + `DeclarePushConstants(0, sizeof(...))` + `Bake()`, `pass.GetBindingSet(0)` returns a non-null handle and `pass.GetPipeline()` returns the same pipeline that was passed in spec. Migrates `RenderPass_OwnedPipeline_BeginReturnsBaseState` (now asserts the data, no longer a state-return).
7. **`RenderPass_BindingTableRegistration`** — pass with `SetBindingTable(1, descriptorTable)` + `Bake()`; `pass.GetBindingTable(1)` returns the same table handle. Covers the Set-1 material-table path currently in Geometry.
8. **`RenderPass_HasNoLifecycleOrStats`** — static_assert: `RenderPass` does NOT expose `Begin/End/Submit/GetStats/GetTime/GetTimeMs`. Pins that the lifecycle + timer + counter drains all left the pass.
9. **`RenderPass_Lifecycle_SurvivesMultipleFrames`** — keep; now drives `rcb.Begin/End/Submit` against an rcb held by the test, and reads `rcb.GetTimeMs(frameIndex)` (was `pass.GetTimeMs`). Verifies timer-query reuse across frames still works in its new home.
10. **`RenderPass_NoPipeline_PassesWork`** — keep the ImGui-shape pass (`RenderPassSpecification{ .Name = "UI" }`); now the pass simply has no pipeline / no baked binding sets — bake is a no-op and GetBindingSet returns null. The ImGui-style lifecycle (rcb.Begin on an empty-spec pass, caller builds state inline, rcb.Submit) still completes. Migrates `RenderPass_NoPipeline_BeginEndSubmitWorks`.

### Tests that get migrated, not deleted

The four existing `RenderPass_*` tests are reshaped, not removed (suite name `Renderer` unchanged). `Pipeline_CustomBlendState_*` is untouched.

---

## Implementation steps (ordered)

Work in the worktree: `C:\Users\niels\Dev\Projects\Eppo\.worktrees\renderpass-merge`.
The worktree is already configured? Check `build/` presence with `Test-Path .worktrees/renderpass-merge/build`. If absent or stale, run `cmake --preset windows-debug` from the worktree root before building.

### Step 0 — umbrella header check + Shader accessor
- Read `EppoEngine/Source/EppoEngine.h`. If `RenderPass.h` is listed there, also add `RenderCommandBuffer.h` (alphabetical / group order). If not, skip.
- Edit `EppoEngine/Source/Renderer/Shader.h`: add `[[nodiscard]] auto GetShaderResources() const -> const std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>>& { return m_ShaderResources; }` inline accessor (one-liner; needed for the Bake reflection lookup in Step 3).

### Step 1 — write/migrate the Renderer tests (RED)
Edit `EppoEngineTesting/Source/Renderer/RendererTests.cpp`:
- Add `#include "Renderer/RenderCommandBuffer.h"`.
- Migrate the four existing `RenderPass_*` tests to the new design (RCB-driven lifecycle, `RenderPass` without `Begin/End/Submit/GetStats/GetTimeMs`).
- Add the new tests listed above (including the static_assert ones).
Build target: `cmake --build --preset windows-debug --target EppoEngineTesting` — **expect compile failure** (RCB/`GetShaderResources` not yet declared, RenderPass API not yet reshaped). This is the RED gate; do not proceed to Step 2 until tests are written.

### Step 2 — implement `RenderCommandBuffer` (GREEN, part 1)
Create `EppoEngine/Source/Renderer/RenderCommandBuffer.h` and `.cpp`. Per the user's clarification of responsibilities:
- Member layout:
  - `nvrhi::CommandListHandle m_CommandList` (created in the RCB constructor; not a Begin parameter).
  - Per-frame `std::vector<nvrhi::TimerQueryHandle> m_TimerQueries` (sized to `MaxFramesInFlight` in the ctor, same pattern as today's RenderPass ctor — `RenderPass.cpp:17-23`).
  - `std::vector<float> m_Timestamps` (likewise sized; written in Submit).
- NO `nvrhi::GraphicsState m_State`. NO `PassStatistics m_Statistics`.
- `Begin(std::string_view name) -> void` — `m_CommandList->open()`, `beginTimerQuery(m_TimerQueries.at(frameIndex))`, `beginMarker(name.data())`. (frameIndex from `DeviceManager::Get()->GetCurrentBackBufferIndex()` — same source RenderPass used.)
- `SetGraphicsState(const nvrhi::GraphicsState& state) -> void` — **single responsibility**: `m_CommandList->setGraphicsState(state)`. Nothing else.
- `[[nodiscard]] GetCommandList() const -> const nvrhi::CommandListHandle&` — returns the held cl for `setPushConstants`/`drawIndexed`/`draw`/`nvrhi::utils::ClearXxx`.
- `End() -> void` — `m_CommandList->endMarker()`, `m_CommandList->endTimerQuery(m_TimerQueries.at(frameIndex))`.
- `Submit() -> void` — `m_CommandList->close()`, `device->executeCommandList(m_CommandList)`, read back timer (`m_Timestamps[frameIndex] = device->getTimerQueryTime(...)`), `device->resetTimerQuery(...)`.
- `[[nodiscard]] GetTime(uint32_t frameIndex) const -> float` + `GetTimeMs(frameIndex)` — same math as `RenderPass` today (`* 1000.0f`).
- Non-copyable (owns per-frame timer queries — copying would double-share them); movable.

Style: Allman braces, 4-space indent, 140-col, pointer-left, `SortIncludes: Never`. Match existing `RenderPass.h` formatting.

### Step 3 — refactor `RenderPass` (GREEN, part 2)
Edit `EppoEngine/Source/Renderer/RenderPass.{h,cpp}`:
- Header: REMOVE `Begin/End/Submit`, `m_TimerQueries`, `m_Timestamps`, `m_Statistics`, `GetStats()` (both overloads), `GetTime/GetTimeMs`. KEEP `RenderPassSpecification m_Specification`, the ctor (no longer creates timer queries), `GetSpecification`/`GetPipeline`/`GetName`/`Resize`.
- NEW binding-recipe storage members (pick the lightest-fuss containers — only 1–2 entries per pass ever, so small fixed-size maps or `std::unordered_map<uint32_t, std::vector</*recipe entry*/>>` are fine):
  - Per-set accumulator for `SetInput` entries (a `(binding, nvrhi::IResource*)` record per registered input).
  - Per-set `BindingSetItem::PushConstants` declaration slot (set → size).
  - Per-set `nvrhi::DescriptorTableHandle` slot (set → table).
  - Per-set `nvrhi::BindingSetHandle` storage (filled by Bake).
- NEW public API:
  - `SetInput(uint32_t set, uint32_t binding, nvrhi::IResource* resource) -> void` — append a `(set, binding, resource)` record.
  - `DeclarePushConstants(uint32_t set, uint32_t size) -> void` — store set → size (replaces any previous declaration for that set).
  - `SetBindingTable(uint32_t set, nvrhi::DescriptorTableHandle table) -> void` — store set → table. (Geometry registers set=1 → material descriptor table.)
  - `Bake() -> void` — for each set that has any recorded entry: query `m_Specification.Pipeline->GetSpecification().Shader->GetShaderResources().at(set)` for the reflected `ShaderResourceBinding` list; for each accumulated `(binding, IResource*)` find the matching `ShaderResourceBinding` (same `Binding` index) to determine `nvrhi::ResourceType`, then construct the right typed `nvrhi::BindingSetItem::X(binding, resource)` (ConstantBuffer / Sampler / StructuredBuffer_SRV / Texture_SRV / etc.); prepend the `PushConstants(binding, size)` entry if declared; assemble a `nvrhi::BindingSetDesc` and call `device->createBindingSet(desc, shader->GetBindingLayouts().at(set))`; store the returned handle. No cache — recreated each frame exactly as today.
  - `[[nodiscard]] GetBindingSet(uint32_t set) const -> const nvrhi::BindingSetHandle&` and `[[nodiscard]] GetBindingTable(uint32_t set) const -> const nvrhi::DescriptorTableHandle&` — return the stored handle/table for the caller's `state.addBindingSet(...)` calls. (ImGui never calls these; its per-draw binding sets stay in its own texture cache.)
- `RenderPass.cpp`: deletes the old `Begin/End/Submit` bodies (their logic moved into the RCB), deletes the timer-query ctor body, adds the new method bodies. Keep `Resize` as-is.

**Bake detail (implementation guide):** when a `(set, binding)` has multiple reflected resource types at the same binding (Geometry Set-0: PushConstants / Sampler / StructuredBuffer_SRV all at slot 0), disambiguate using `IResource*`'s concrete type — `dynamic_cast`/`Query` nvrhi resource interface (`nvrhi::IBuffer` → ConstantBuffer or StructuredBuffer_SRV depending on reflection's ResourceType/usage flags; `nvrhi::ISampler` → Sampler; the existing call sites already encode exactly this rule). Defer the precise mechanism to the implementation — the goal is one consistent mapping that reproduces what today's typed `BindingSetItem::X(...)` already does, not a clever new abstraction.

### Step 4 — migrate `SceneRenderer` (GREEN, part 3)
Edit `EppoEngine/Source/Renderer/SceneRenderer.{h,cpp}`:
- Header: add `RenderCommandBuffer m_GeometryRCB, m_SkyRCB, m_WireframeRCB;` and `PassStatistics m_GeometryStats, m_SkyStats, m_WireframeStats;`. Remove the now-stale references to pass-stats accessors from RenderGui.
- `EndScene` (or wherever the per-frame descriptor-table write happens today): keep the existing `descriptorTable` resize+write logic; the actual SetInput/DeclarePushConstants/Bake calls move to the per-pass functions (see Step 4 example below) OR happen here if cleaner — pick whichever results in the smallest diff. The original plan suggested EndScene; either is fine.

- `GeometryPass()` rewrite sketch (replaces today's `scene_renderer.cpp:347-438` block):
  ```cpp
  m_GeometryStats = {};                                   // was pass.m_Statistics = {} in Begin

  m_GeometryRCB.Begin("Geometry");

  // Clears: RenderPass owns the spec flags + pipeline, but the cl is on the RCB.
  const auto& pipeline = m_GeometryPass.GetPipeline();
  const auto& framebuffer = pipeline->GetSpecification().Framebuffer->GetFramebuffer();
  if (m_GeometryPass.GetSpecification().ClearColor)
      nvrhi::utils::ClearColorAttachment(m_GeometryRCB.GetCommandList(), framebuffer, 0, /*clearColor from fb spec*/);
  if (m_GeometryPass.GetSpecification().ClearDepth)
      nvrhi::utils::ClearDepthStencilAttachment(m_GeometryRCB.GetCommandList(), framebuffer, /*spec*/);

  // Register per-frame inputs and bake (was the createBindingSet + addBindingSet block).
  m_GeometryPass.DeclarePushConstants(0, sizeof(PushConstants));
  m_GeometryPass.SetInput(0, 1, m_CameraUB->GetBuffer());
  m_GeometryPass.SetInput(0, 2, m_LightsUB->GetBuffer());
  m_GeometryPass.SetInput(0, 3, m_EnvironmentUB->GetBuffer());
  m_GeometryPass.SetInput(0, 0, m_Sampler);                                  // sampler at binding 0
  m_GeometryPass.SetInput(0, 0, m_InstanceTransformsSB->GetBuffer());         // StructuredBuffer_SRV at binding 0
  m_GeometryPass.SetBindingTable(1, descriptorTable);
  m_GeometryPass.Bake();

  // Build the base GraphicsState locally (was returned by pass.Begin).
  const auto width  = static_cast<float>(pipeline->GetWidth());
  const auto height = static_cast<float>(pipeline->GetHeight());
  nvrhi::GraphicsState state{};
  state.pipeline      = pipeline->GetPipeline();
  state.framebuffer   = framebuffer;
  state.viewport.viewports   = { nvrhi::Viewport(width, height) };
  state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(width), static_cast<int>(height)) };
  state.addBindingSet(m_GeometryPass.GetBindingSet(0));
  state.addBindingSet(m_GeometryPass.GetBindingTable(1));

  m_GeometryRCB.SetGraphicsState(state);

  PushConstants pushConstants{};
  for (const auto& drawCmd : m_DrawCommands | std::views::values)
  {
      const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
      if (instanceCount == 0) continue;

      for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
      {
          // Per-submesh state mutation + re-push (preserves today's setGraphicsState cadence).
          const nvrhi::VertexBufferBinding vtxBufBinding{ submesh.VertexBuffer->GetBuffer(), 0, 0 };
          state.vertexBuffers.assign(1, vtxBufBinding);
          state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
          state.indexBuffer.format = nvrhi::Format::R32_UINT;
          state.indexBuffer.offset = 0;
          m_GeometryRCB.SetGraphicsState(state);

          pushConstants.Transform = submesh.LocalTransform;
          pushConstants.InstanceOffset = drawCmd.InstanceOffset;

          for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
          {
              pushConstants.DiffuseMapIndex = material->DiffuseMapIndex;
              pushConstants.NormalMapIndex   = material->NormalMapIndex;
              pushConstants.RoughMetMapIndex = material->RoughMetMapIndex;
              pushConstants.Metallic  = material->Metallic;
              pushConstants.Roughness = material->Roughness;

              m_GeometryRCB.GetCommandList()->setPushConstants(&pushConstants, sizeof(PushConstants));

              nvrhi::DrawArguments drawArgs{
                  .vertexCount = static_cast<uint32_t>(indexCount),
                  .instanceCount = instanceCount,
                  .startIndexLocation = firstIndex,
                  .startVertexLocation = firstVertex,
              };
              m_GeometryRCB.GetCommandList()->drawIndexed(drawArgs);

              m_GeometryStats.DrawCalls++;
              m_GeometryStats.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
              m_GeometryStats.Indices  += static_cast<uint32_t>(indexCount) * instanceCount;
          }
          m_GeometryStats.Submeshes++;
      }
      m_GeometryStats.Instances += instanceCount;
      m_GeometryStats.Meshes++;
  }

  m_GeometryRCB.End();
  m_GeometryRCB.Submit();
  ```
- `SkyPass()`: drop the `state = pass.Begin(cl)` pattern; similar shape — build state locally, register+bake on `m_SkyPass` (Set-0: CameraUB @ binding 1, EnvironmentUB @ binding 3; no PushConstants, no Set-1), one `m_SkyRCB.SetGraphicsState(state)` before the single `draw`, manual `m_SkyStats.DrawCalls++ / Vertices += 3`.
- `WireframePass()`: same shape as Geometry (per-submesh `SetGraphicsState`, per-primitive `setPushConstants`+`drawIndexed` on `m_WireframeRCB.GetCommandList()`, manual `m_WireframeStats++`). Set-0: `DeclarePushConstants(0, sizeof(PushConstants))` + `SetInput(0, 1, m_CameraUB)` + `SetInput(0, 0, m_WireframeInstanceSB)`.
- `RenderGui`: replace `m_GeometryPass.GetStats()` etc. with `m_GeometryStats` etc.; replace `m_GeometryPass.GetTimeMs(frameIndex)` with `m_GeometryRCB.GetTimeMs(frameIndex)`.

### Step 5 — migrate `ImGuiRenderer` (GREEN, part 4)
Edit `EppoEngine/Source/ImGui/ImGuiRenderer.{h,cpp}`:
- Header: add `RenderCommandBuffer m_RCB;` + `PassStatistics m_Stats;` (replaces reading from `m_Pass`). Keep `RenderPass m_Pass{ RenderPassSpecification{ .Name = "UI" } }`.
- `Render()` body (`ImGuiRenderer.cpp:149-234` area): replace
  ```cpp
  nvrhi::GraphicsState state = m_Pass.Begin(m_CommandList);
  m_Statistics = {};
  // ... clear via m_CommandList ...
  // ... state.pipeline/fram`viewport/etc ... setup ...
  ```
  with
  ```cpp
  m_Stats = {};
  m_RCB.Begin("UI");
  // Existing manual clear, now using m_RCB.GetCommandList():
  nvrhi::utils::ClearColorImage(m_RCB.GetCommandList(), ...);   // kept shape, source cl changed
  ```
  then build the per-viewport `nvrhi::GraphicsState state` locally exactly as today (`pipeline = GetOrCreatePipeline(...)/framebuffer/viewport/vertexBuffers/indexBuffer`), call `m_RCB.SetGraphicsState(state)` once before the per-drawCmd loop. Per drawCmd mutate the same `state` (`state.bindings = { GetOrCreateBindingSet(texture) }; state.viewport.scissorRects[0] = rect;`) and re-`m_RCB.SetGraphicsState(state)`. Per-draw `m_RCB.GetCommandList()->setPushConstants(&pushConstants, ...); m_RCB.GetCommandList()->drawIndexed(drawArgs);` + `m_Stats.DrawCalls++;` (and `Vertices/Indices` at viewport-end as today).
- End: `m_RCB.End(); m_RCB.Submit();`.
- `GetOwnGPUTime` / `GetGPUTime`: read `m_RCB.GetTimeMs(frameIndex)` (was `m_Pass.GetTimeMs`).
- `GetOwnStats` / `GetStats`: read `m_Stats` (was `m_Pass.GetStats()`).
- ImGui's `m_Pass` (pipeline-less) is never `Bake`d and never read via `GetBindingSet` — it stays a pure spec holder for ImGuiRenderer's own bookkeeping (mostly the name). This matches today's no-pipeline path; pass-vs-rcb split is asymmetric for ImGui by design (texture cache stays in ImGuiRenderer).

### Step 6 — build & verify (GREEN gate)
From the worktree root:
```bash
cmake --build --preset windows-debug --target EppoEngineTesting
```
Must compile clean. Fix clang-tidy / clang-format findings (run `clang-format` on touched files).

Then run the Renderer suite (graphical — needs a display):
```bash
ctest --test-dir .worktrees/renderpass-merge/build/debug -R Renderer --output-on-failure
```
Also build the editor to ensure no orphaned call sites:
```bash
cmake --build --preset windows-debug --target EppoEditor
```

### Step 7 — code review (per workflow)
`git -C .worktrees/renderpass-merge diff` the full change. Verify:
- No `PassStatistics m_Statistics` / `GetStats()` accessor on either `RenderPass` OR `RenderCommandBuffer`.
- No `Begin/End/Submit` / `GetTime/GetTimeMs` / timer queries on `RenderPass`.
- No `setPushConstants` / `drawIndexed` / `Draw` / `DrawIndexed` / `SetVertexBuffers` / `SetIndexBuffer` / `SetScissor` / `SetViewport` / `AddBindingSet` / `SetBindings` methods on `RenderCommandBuffer` — only `SetGraphicsState` + lifecycle + `GetCommandList` + `GetTime/GetTimeMs`.
- `RenderPass::SetInput`/`DeclarePushConstants`/`SetBindingTable`/`Bake`/`GetBindingSet`/`GetBindingTable` exist and replace the 3 duplicated `createBindingSet + addBindingSet` blocks.
- Per-submesh and per-draw `setGraphicsState` cadence in SceneRenderer/ImGuiRenderer matches today (no per-primitive `setGraphicsState` regression — Issue 1 from the review doc is honored).
- Comments explain "why" (single-responsibility split, why RCB owns no stats, why ImGui's pass is never baked) — match the existing concise-why-comments style.

### Step 8 — do NOT commit
User commits manually. Do not run `git commit` unless explicitly asked.

---

## Risks / things to watch

- **Bake's reflection-based type disambiguation.** Geometry's Set-0 declares `PushConstants(0)`, `Sampler(0)`, `StructuredBuffer_SRV(0)` — three items at binding 0 across different resource types. The current code knows the type by calling the typed `BindingSetItem::X` constructor. SetInput deliberately accepts a type-erased `IResource*`, so Bake must reconstruct the typed item via reflection. If reflection doesn't expose enough to disambiguate reliably, fall back to typed `SetInput` overloads (e.g. `SetInput(uint32_t set, uint32_t binding, nvrhi::IBuffer*)` vs `SetInput(..., nvrhi::ISampler*)`) — pick whichever most faithfully reproduces today's typed inventor at lowest cost. Note as a known risk to resolve during Step 3 implementation, not a blocker for the plan.
- **`SetInput` ordering / last-write-wins.** If two `SetInput(set, binding, ...)` calls register different resources at the same `(set, binding)` (e.g. last call wins) — caller-side discipline, but document it. Today's `BindingSetDesc.bindings` is ordered; preserve insertion order for the desc Bake builds.
- **RCB lifetime across frames.** RCB is persistent and now owns the per-frame timer queries. The RCB ctor must size `m_TimerQueries` to `MaxFramesInFlight` (same pattern as today's RenderPass ctor at `RenderPass.cpp:17-23`). `Submit` must read back into `m_Timestamps[frameIndex]` and call `resetTimerQuery` — same sequence as today, just in the new owner.
- **ImGui viewport-state leak.** ImGuiRenderer's `Render` is called per viewport and its `m_RCB` is persistent. Each `m_RCB.Begin("UI")` re-opens the cl + restarts the marker; ImGui then re-builds `state` from scratch per viewport using its own locals (pipeline/framebuffer/viewport/binding/scissor). No `m_State` on the RCB means there is nothing to leak — the previous viewport's state variable goes out of scope naturally. This is *simpler* than the prior design (which held state on the RCB).
- **`EppoEngine.h` include.** If the umbrella header lists per-file includes, missing `RenderCommandBuffer.h` breaks external consumers. Check in Step 0.
- **`Shader::GetShaderResources` access.** Needs to be `const` + return `const&`; `m_ShaderResources` is filled at Shader construction and never mutated after, so exposing it read-only is safe.

---

## File list (touches)

**New:**
- `EppoEngine/Source/Renderer/RenderCommandBuffer.h`
- `EppoEngine/Source/Renderer/RenderCommandBuffer.cpp`

**Modified:**
- `EppoEngine/Source/Renderer/RenderPass.h`
- `EppoEngine/Source/Renderer/RenderPass.cpp`
- `EppoEngine/Source/Renderer/SceneRenderer.h`
- `EppoEngine/Source/Renderer/SceneRenderer.cpp`
- `EppoEngine/Source/ImGui/ImGuiRenderer.h`
- `EppoEngine/Source/ImGui/ImGuiRenderer.cpp`
- `EppoEngine/Source/Renderer/Shader.h` (one-line `GetShaderResources()` accessor)
- `EppoEngine/Source/EppoEngine.h` (only if Step 0 finds `RenderPass.h` listed)
- `EppoEngineTesting/Source/Renderer/RendererTests.cpp`

**No CMake changes** (GLOB_RECURSE picks up new files; suite already registered).

---

## Handoff notes for the implementing agent

- You are on branch `refactor/renderpass-merge` in worktree `.worktrees/renderpass-merge`. The base commit `29d8604` already merged the RenderPass scope/lifecycle refactor; this PR builds on top.
- Follow the workflow rules in `AGENTS.md`: plan is done (this doc); TDD (tests first, RED→GREEN); systematic debugging if anything breaks (find root cause, don't patch symptoms); code review the diff before declaring done; verify with the build + ctest before claiming complete.
- **Architecture corrected mid-planning**: the original draft put lifecycle on `RenderPass` and held `GraphicsState`/`PassStatistics` on the RCB. The user clarified the inverse — RCB owns lifecycle (cl/marker/timer/Begin/End/Submit), RenderPass owns only data (spec + binding-set recipe via `SetInput`/`DeclarePushConstants`/`SetBindingTable`/`Bake`), and the manual CPU `PassStatistics` lives on the **renderer** (one member per owned pass), NOT on either RCB or RenderPass. `nvrhi::GraphicsState` lives in the caller's local — the RCB exposes only `SetGraphicsState(state)` (single responsibility — no PC/draw bundling) and `GetCommandList()` for the few ops not wrapped.
- **Critical correctness constraints**: do NOT add `DrawIndexed`/`Draw`/`setPushConstants`/SetVertexBuffers/`SetIndexBuffer`/`SetScissor`/`SetViewport`/`AddBindingSet`/`SetBindings` to the RCB. `SetGraphicsState` MUST do only `m_CommandList->setGraphicsState(state)` — nothing else. The user named this explicitly; `ApplyState` or similar merge-named APIs are explicitly rejected ("nothing gets applied — the graphics state gets set, period").
- `Renderer::BeginRenderPass/EndRenderPass/RenderMesh` is a follow-up PR. Do NOT introduce it here.
- Do NOT add a binding-set cache. The user rejected it as overengineered. Recreate via `Bake()` each frame, same cost as today.
- Run clang-format on every touched file before considering each step done. Match the existing style exactly (Allman, 4-space, 140-col, pointer-left, `SortIncludes: Never`).
- When done: report what was built + which tests passed (with the actual ctest output), then stop. Do not commit.