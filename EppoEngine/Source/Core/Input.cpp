#include "pch.h"
#include "Core/Input.h"

#include "Core/Application.h"
#include "Core/InputBackend.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	namespace
	{
		// Default backend: reads the live GLFW window through the application
		// singleton. Behaviour matches the pre-seam Input implementation.
		class GlfwInputBackend final : public InputBackend
		{
		public:
			[[nodiscard]] auto IsKeyPressed(KeyCode key) const -> bool override
			{
				GLFWwindow* window = Application::Get().GetWindow()->GetNative();
				return glfwGetKey(window, key) == GLFW_PRESS;
			}

			[[nodiscard]] auto IsMouseButtonPressed(MouseCode button) const -> bool override
			{
				GLFWwindow* window = Application::Get().GetWindow()->GetNative();
				return glfwGetMouseButton(window, button) == GLFW_PRESS;
			}

			[[nodiscard]] auto GetMousePosition() const -> glm::vec2 override
			{
				GLFWwindow* window = Application::Get().GetWindow()->GetNative();

				double xPos, yPos;
				glfwGetCursorPos(window, &xPos, &yPos);

				return { static_cast<float>(xPos), static_cast<float>(yPos) };
			}
		};

		auto DefaultBackend() -> InputBackend&
		{
			static GlfwInputBackend backend;
			return backend;
		}
	}

	InputBackend* Input::s_Backend = nullptr;

	auto Input::SetBackend(InputBackend* backend) -> void
	{
		s_Backend = backend;
	}

	auto Input::GetBackend() -> InputBackend&
	{
		return s_Backend ? *s_Backend : DefaultBackend();
	}

	auto Input::IsKeyPressed(KeyCode key) -> bool
	{
		return GetBackend().IsKeyPressed(key);
	}

	auto Input::IsMouseButtonPressed(MouseCode button) -> bool
	{
		return GetBackend().IsMouseButtonPressed(button);
	}

	auto Input::GetMousePosition() -> glm::vec2
	{
		return GetBackend().GetMousePosition();
	}

	auto Input::GetMouseX() -> float
	{
		return GetBackend().GetMousePosition().x;
	}

	auto Input::GetMouseY() -> float
	{
		return GetBackend().GetMousePosition().y;
	}
}
