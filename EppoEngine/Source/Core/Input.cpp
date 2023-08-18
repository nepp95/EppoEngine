#include "pch.h"
#include "Input.h"

#include "Renderer/RendererContext.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	bool Input::IsKeyPressed(KeyCode key)
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		int keyState = glfwGetKey(window, key);

		return keyState == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		int buttonState = glfwGetMouseButton(window, button);

		return buttonState == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePosition()
	{
		GLFWwindow* window = RendererContext::Get()->GetWindowHandle();
		double xPos, yPos;
		glfwGetCursorPos(window, &xPos, &yPos);

		return glm::vec2((float)xPos, (float)yPos);
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
