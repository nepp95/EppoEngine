#pragma once

#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class InputBackend;

	// Static facade for input queries. Delegates to the currently installed
	// InputBackend; by default that reads the live GLFW window, so existing call
	// sites are unchanged. Tests and automated scenarios call SetBackend to swap
	// in a SimulatedInput.
	class Input
	{
	public:
		static auto IsKeyPressed(KeyCode key) -> bool;
		static auto IsMouseButtonPressed(MouseCode button) -> bool;

		static auto GetMousePosition() -> glm::vec2;
		static auto GetMouseX() -> float;
		static auto GetMouseY() -> float;

		// Ungated query: reports the backend state regardless of the world-input
		// gate below. Editor chrome (e.g. Ctrl+S / Ctrl+O accelerators) uses this so
		// it keeps responding even when the viewport doesn't own gameplay input.
		[[nodiscard]] static auto IsKeyPressedRaw(KeyCode key) -> bool;

		// World-input gate. While disabled, IsKeyPressed and IsMouseButtonPressed
		// report no input, so everything that drives the scene from polled input
		// (the editor camera, running scripts) stays quiet when the viewport isn't
		// focused. Defaults to enabled, so non-editor hosts and tests are unaffected.
		static auto SetViewportInputEnabled(bool enabled) -> void;
		[[nodiscard]] static auto IsViewportInputEnabled() -> bool;

		// Install a backend (non-owning; the caller keeps ownership). Passing
		// nullptr reverts to the default GLFW-backed source. The caller MUST call
		// SetBackend(nullptr) before the backend is destroyed — the stored pointer
		// is otherwise left dangling. Intended for main-thread use only (the swap
		// is unsynchronized).
		static auto SetBackend(InputBackend* backend) -> void;
		[[nodiscard]] static auto GetBackend() -> InputBackend&;

	private:
		static InputBackend* s_Backend;
		static bool s_ViewportInputEnabled;
	};
}
