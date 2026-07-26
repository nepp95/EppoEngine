#include "pch.h"
#include "Core/Input.h"

#include "Core/Application.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	bool Input::s_ViewportInputEnabled = true;

	auto Input::SetViewportInputEnabled(bool enabled) -> void
	{
		s_ViewportInputEnabled = enabled;
	}

	auto Input::IsViewportInputEnabled() -> bool
	{
		return s_ViewportInputEnabled;
	}

	auto Input::IsKeyPressed(KeyCode key) -> bool
	{
		return s_ViewportInputEnabled && IsKeyPressedRaw(key);
	}

	auto Input::IsKeyPressedRaw(KeyCode key) -> bool
	{
		GLFWwindow* window = Application::Get().GetWindow()->GetNative();
		return glfwGetKey(window, key) == GLFW_PRESS;
	}

	auto Input::IsMouseButtonPressed(MouseCode button) -> bool
	{
		if (!s_ViewportInputEnabled)
			return false;

		GLFWwindow* window = Application::Get().GetWindow()->GetNative();
		return glfwGetMouseButton(window, button) == GLFW_PRESS;
	}

	auto Input::GetMousePosition() -> glm::vec2
	{
		GLFWwindow* window = Application::Get().GetWindow()->GetNative();

		double xPos, yPos;
		glfwGetCursorPos(window, &xPos, &yPos);

		return { static_cast<float>(xPos), static_cast<float>(yPos) };
	}

	auto Input::GetMouseX() -> float
	{
		return GetMousePosition().x;
	}

	auto Input::GetMouseY() -> float
	{
		return GetMousePosition().y;
	}
}
