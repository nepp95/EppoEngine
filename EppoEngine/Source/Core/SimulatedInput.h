#pragma once

#include "Core/InputBackend.h"

namespace Eppo
{
	// A programmable InputBackend for tests and automated scenarios: key/button
	// state and cursor position are set explicitly rather than read from a
	// device. Install it with Input::SetBackend(&sim) and revert to the live
	// device with Input::SetBackend(nullptr).
	class SimulatedInput final : public InputBackend
	{
	public:
		auto PressKey(KeyCode key) -> void { m_Keys.insert(key); }
		auto ReleaseKey(KeyCode key) -> void { m_Keys.erase(key); }
		auto PressMouseButton(MouseCode button) -> void { m_Buttons.insert(button); }
		auto ReleaseMouseButton(MouseCode button) -> void { m_Buttons.erase(button); }
		auto SetMousePosition(const glm::vec2& position) -> void { m_MousePosition = position; }

		// Clear all pressed keys/buttons and reset the cursor to the origin.
		auto Reset() -> void
		{
			m_Keys.clear();
			m_Buttons.clear();
			m_MousePosition = glm::vec2(0.0f);
		}

		[[nodiscard]] auto IsKeyPressed(KeyCode key) const -> bool override { return m_Keys.contains(key); }
		[[nodiscard]] auto IsMouseButtonPressed(MouseCode button) const -> bool override { return m_Buttons.contains(button); }
		[[nodiscard]] auto GetMousePosition() const -> glm::vec2 override { return m_MousePosition; }

	private:
		std::unordered_set<KeyCode> m_Keys;
		std::unordered_set<MouseCode> m_Buttons;
		glm::vec2 m_MousePosition{ 0.0f };
	};
}
