#include "pch.h"
#include "Core/Window.h"

#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"

#include <GLFW/glfw3.h>
#include <nfd.hpp>
#include <nfd_glfw3.h>
#include <stb_image.h>

namespace Eppo
{
	namespace
	{
		auto GLFWErrorCallback(int error, const char* description) -> void
		{
			Log::Error(LogSource::Glfw, "({}) {}", error, description);
		}
	}

	Window::Window(const uint32_t width, const uint32_t height)
		: m_Width(width), m_Height(height)
	{
		// Setup glfw
		int success = glfwInit();
		EP_ASSERT(success, "GLFW failed to initialize!");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
		glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
		glfwSetErrorCallback(GLFWErrorCallback);

		// Create window
		Log::Info("Creating window of size {}x{}", m_Width, m_Height);
		m_Window = glfwCreateWindow(static_cast<int>(m_Width), static_cast<int>(m_Height), "EppoEngine", nullptr, nullptr);

		// Init file dialogs
		success = NFD::Init();
		EP_ASSERT(success == NFD_OKAY);

		// Set event callbacks
		glfwSetWindowUserPointer(m_Window, &m_EventCallback);

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
				WindowCloseEvent e;
				callback(e);
			}
		);

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, const int width, const int height) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
				WindowResizeEvent e(width, height);
				callback(e);
			}
		);

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, const int key, const int scancode, const int action, const int mods) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

				switch (action)
				{
					case GLFW_PRESS:
					{
						KeyPressedEvent e(key);
						callback(e);
						break;
					}

					case GLFW_RELEASE:
					{
						KeyReleasedEvent e(key);
						callback(e);
						break;
					}

					case GLFW_REPEAT:
					{
						KeyPressedEvent e(key, true);
						callback(e);
						break;
					}
				}
			}
		);

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, const unsigned int key) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
				KeyTypedEvent e(key);
				callback(e);
			}
		);

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

				switch (action)
				{
					case GLFW_PRESS:
					{
						MouseButtonPressedEvent e(button);
						callback(e);
						break;
					}

					case GLFW_RELEASE:
					{
						MouseButtonReleasedEvent e(button);
						callback(e);
						break;
					}
				}
			}
		);

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
				MouseScrolledEvent e(static_cast<float>(xOffset), static_cast<float>(yOffset));
				callback(e);
			}
		);

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) -> void
			{
				const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
				MouseMovedEvent e(static_cast<float>(xPos), static_cast<float>(yPos));
				callback(e);
			}
		);
	}

	auto Window::Shutdown() -> void
	{
		NFD::Quit();
		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	auto Window::ProcessEvents() -> void
	{
		glfwPollEvents();
	}

	auto Window::SetIcon(const std::filesystem::path& path) -> void
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels)
		{
			Log::Error("Failed to load window icon from '{}'", path);
			return;
		}

		GLFWimage image;
		image.width = width;
		image.height = height;
		image.pixels = pixels;
		glfwSetWindowIcon(m_Window, 1, &image);

		stbi_image_free(pixels);
	}
}