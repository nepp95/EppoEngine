# Application framework architecture

## Startup and ownership

`Core/EntryPoint.h` supplies `main` and calls the application-specific `CreateApplication(argc, argv)`. `EppoEditor/Source/EppoEditor.cpp` implements that factory.

`Application` is a singleton during its lifetime and owns, in dependency order:

- `Window` and its platform backend;
- `DeviceManager` and NVRHI renderer;
- application layers;
- `ImGuiLayer` and its renderer integration.

`Layer` exposes attach, detach, update, UI render, and event hooks. `PushLayer` constructs a layer, stores shared ownership, and immediately calls `OnAttach`.

## Construction order

1. Set the singleton and initialize logging/profiling prerequisites.
2. Create the GLFW-backed `Window` and install the application event callback.
3. Create the API-specific `DeviceManager`; the Vulkan backend gathers GLFW's required instance extensions while constructing its instance.
4. Initialize the device manager's surface and swapchain.
5. Initialize `Renderer` after the NVRHI device is live.
6. Create/attach `ImGuiLayer` after renderer services exist.
7. Let the application factory push editor/runtime layers.

Reverse dependency order during destruction. Wait for GPU idle before releasing GPU users when required.

## Frame order

`Run` computes a wall-clock timestep and repeatedly calls `StepFrame`. `StepFrame` exists so tests can drive deterministic fixed timesteps and frame counts.

The effective frame phases are:

1. Poll window events.
2. If minimized, avoid normal device/update/present work.
3. Begin/acquire the device frame.
4. Call `OnUpdate(timestep)` on layers in insertion order.
5. Begin ImGui.
6. Call `OnUIRender()` on layers.
7. End/render ImGui.
8. Present the device frame.

Respect a failed `BeginFrame`; do not record or present against an unavailable swapchain image.

## Event flow

Window callbacks construct typed events such as resize, close, key, mouse button, mouse move, and scroll. `Application::OnEvent` first dispatches application-owned events, then forwards remaining events through the layer stack in insertion order. `ImGuiLayer` is pushed during application construction, so this order lets it capture input before later layers. Stop propagation when `Handled` becomes true.

Window close marks the app not running. Resize currently updates minimized state only; swapchain recreation is handled by its own acquire/present behavior rather than directly from `Application::OnWindowResize`.

When adding an event:

1. Define its type/category and payload under `Event`.
2. Emit it from the platform window callback.
3. Update stateful input backend data if applicable.
4. Handle it in application/ImGui/layers in the correct priority order.
5. Add unit or application-harness coverage.

## Input model

`Input` exposes static polled queries through an `InputBackend`. The normal backend reads platform/GLFW state. `SimulatedInput` supports deterministic tests and controlled scenarios.

Editor code gates polled input through `Input::SetViewportInputEnabled`: editor camera and running scripts should remain inactive while users type or click in other panels. Event delivery and polled input are related but not interchangeable; preserve both when adding keys/buttons.

Key and mouse numeric values are shared with C# scripting. Update `Core/KeyCodes.h`, managed `KeyCodes.cs`, platform mapping, and tests together when changing them.

## Window and filesystem assumptions

`Window` owns the native GLFW window and provides framebuffer size, event callback, VSync/fullscreen/decorated state, native handle, and icon operations. Vulkan surface extensions and framebuffer sizing originate here.

Runtime files resolve relative to the executable output directory. `FS::GetRootDirectory` and `FS::GetResourcesDirectory` depend on that layout. Editor/tests must run from the executable output directory so `Resources`, shader includes/cache, `runtimeconfig.json`, and managed DLLs are found.

## ImGui integration

`ImGuiLayer` owns context/frame setup, docking and multi-viewport configuration, event blocking policy, and `ImGuiRenderer`. `ImGuiRenderer` translates draw lists into NVRHI buffers, pipeline bindings, scissor rectangles, texture descriptors, command recording, and swapchain framebuffer output.

Keep ImGui GPU resources synchronized with back-buffer count and viewport/swapchain changes. Application UI phase must enclose every layer's `OnUIRender`.

## Core conventions

`Core/Base.h` defines the ownership and style vocabulary used across the engine: `Ref<T>`/`CreateRef` (shared_ptr), `ScopedPtr<T>`/`CreateScopedPtr` (unique_ptr), `EP_ASSERT(cond, msg)`, config macros `EP_DEBUG`/`EP_RELEASE`/`EP_DIST`, and Tracy profiling (`EP_PROFILE_FN`). Engine code uses trailing-return-type style (`auto Foo() -> void`). Sibling core utilities: `Core/Log.h`, `UUID.h`, `Buffer.h`, `Hash.h`.

## Testing infrastructure

`EppoEngineTesting/Source/Support/AppHarness` boots a real `Application`, window, device, renderer, and resources. It can advance a deterministic number of frames. `TestContext` and `ScenarioLayer` support higher-level scenarios and simulated input.

Test routing:

- headless `Core`: buffers, hashes, UUIDs, and isolated non-window logic;
- graphical `App`: boot, live window/device, and repeated frame advancement;
- graphical `Scenario`: state changes across frames, editor camera, input, scene loading, and rendering;
- graphical `Renderer`: direct GPU abstraction behavior.

Graphical suites require a real display and GPU and are excluded by headless CI. Run them from CTest so the configured working directory is correct.

## Change checklist

For frame/startup changes, verify construction and destruction order, minimized and failed-acquire paths, repeated fixed-step frames, and renderer availability. For input/event changes, verify native callbacks, event propagation/handling, polled state, simulated state, viewport gating, and managed key-code parity. For ImGui changes, verify application frame bracketing, back-buffer resource ownership, docking/multi-viewport behavior, and resize.
