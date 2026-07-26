#pragma once

#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

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
		static bool s_ViewportInputEnabled;
	};
}
