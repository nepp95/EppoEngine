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

		// Install a backend (non-owning; the caller keeps ownership). Passing
		// nullptr reverts to the default GLFW-backed source. The caller MUST call
		// SetBackend(nullptr) before the backend is destroyed — the stored pointer
		// is otherwise left dangling. Intended for main-thread use only (the swap
		// is unsynchronized).
		static auto SetBackend(InputBackend* backend) -> void;
		[[nodiscard]] static auto GetBackend() -> InputBackend&;

	private:
		static InputBackend* s_Backend;
	};
}
