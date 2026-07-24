# Adding an event type

Events live in `EppoEngine/Source/Event/`. An event is a class that declares its
type via the `EVENT_CLASS_TYPE` macro, is dispatched from wherever the condition
arises, and is handled in a layer's `OnEvent`. Grep the named symbols.

## Touchpoints

1. **`Event/Event.h`** — add the value to the `enum class EventType`. Categories
   (`EventCategoryApplication`, `…Input`, `…Keyboard`, `…Mouse`, `…MouseButton`)
   are a bitmask; reuse or add one if the event needs category filtering.

2. **The event class** — put it in the matching header (`ApplicationEvent.h`,
   `KeyEvent.h`, `MouseEvent.h`, or a new one). Use the `EVENT_CLASS_TYPE(type)`
   macro so `GetStaticType`/`GetEventType` line up — the dispatcher matches on
   `GetStaticType`, so a mismatch means handlers never fire. Set category flags.

3. **The dispatch site** — construct and raise the event where the condition
   occurs. Window/GLFW callbacks in `Core/Window.cpp` are the usual origin for
   input/window events; feed it into the layer stack via the window's event
   callback.

4. **Handlers** — consume it in the relevant `Layer::OnEvent` (e.g.
   `EditorLayer::OnEvent`, `RuntimeLayer::OnEvent`, `ImGuiLayer`) using
   `EventDispatcher::Dispatch<T>`. An event nobody dispatches on is inert.

See the `eppo-application-framework` skill for event flow and ordering.

## Verify

Build + `ctest -R App` (graphical; needs a display). Confirm the handler runs and
that `event.Handled` propagation through the layer stack is what you intend.
