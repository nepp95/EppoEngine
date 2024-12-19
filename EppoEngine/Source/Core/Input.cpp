#include "pch.h"
#include "Input.h"

#include "Renderer/RendererContext.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	bool Input::IsKeyPressed(const KeyCode key)
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		const int keyState = glfwGetKey(window, key);

		return keyState == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		const int buttonState = glfwGetMouseButton(window, button);

		return buttonState == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePosition()
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		double xPos, yPos;
		glfwGetCursorPos(window, &xPos, &yPos);

		return { static_cast<float>(xPos), static_cast<float>(yPos) };
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}
}
