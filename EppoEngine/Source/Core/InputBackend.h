#pragma once

#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

#include <glm/glm.hpp>

namespace Eppo
{
	// Abstraction over the source of input state. The default backend reads the
	// live GLFW window; tests and automated scenarios install a SimulatedInput to
	// drive input programmatically. Input delegates its queries to the currently
	// installed backend (see Input::SetBackend).
	//
	// GetMouseX/Y are derived from GetMousePosition by the Input layer, so a
	// backend only needs to report the full cursor position.
	class InputBackend
	{
	public:
		virtual ~InputBackend() = default;

		[[nodiscard]] virtual auto IsKeyPressed(KeyCode key) const -> bool = 0;
		[[nodiscard]] virtual auto IsMouseButtonPressed(MouseCode button) const -> bool = 0;
		[[nodiscard]] virtual auto GetMousePosition() const -> glm::vec2 = 0;
	};
}
