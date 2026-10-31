#pragma once

#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"
#include "Core/Window.h"

#include <glm/glm.hpp>

namespace Eppo
{
    class Input
    {
    public:
        static auto IsKeyPressed(KeyCode key) -> bool;
        static auto IsMouseButtonPressed(MouseCode button) -> bool;

        static auto GetMousePosition() -> glm::vec2;
        static auto GetMouseX() -> float;
        static auto GetMouseY() -> float;

        // Viewport the scene is presented in, in main-window client coordinates (the
        // same space as GetMousePosition). The editor sets this every frame to the
        // viewport panel's image rect; when never set, the whole window is the viewport,
        // which is what a fullscreen runtime presents into.
        static auto SetViewportBounds(const glm::vec2& min, const glm::vec2& max) -> void;
        [[nodiscard]] static auto GetViewportSize() -> glm::vec2;

        // Mouse position relative to the viewport's top-left corner, in pixels.
        [[nodiscard]] static auto GetViewportMousePosition() -> glm::vec2;

        static auto SetCursorMode(CursorMode mode) -> void;
        [[nodiscard]] static auto GetCursorMode() -> CursorMode;

        // Ungated query: reports the live state regardless of the world-input gate
        // below. Editor chrome (e.g. Ctrl+S / Ctrl+O accelerators) uses this so it
        // keeps responding even when the viewport doesn't own gameplay input.
        [[nodiscard]] static auto IsKeyPressedRaw(KeyCode key) -> bool;

        // World-input gate. While disabled, IsKeyPressed and IsMouseButtonPressed
        // report no input, so everything that drives the scene from polled input
        // (the editor camera, running scripts) stays quiet when the viewport isn't
        // focused. Defaults to enabled, so non-editor hosts are unaffected.
        static auto SetViewportInputEnabled(bool enabled) -> void;
        [[nodiscard]] static auto IsViewportInputEnabled() -> bool;

    private:
        [[nodiscard]] static auto GetViewportMin() -> glm::vec2;

        static bool s_ViewportInputEnabled;
        static bool s_HasViewportBounds;
        static glm::vec2 s_ViewportMin;
        static glm::vec2 s_ViewportMax;
    };
}
