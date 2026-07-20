#include "pch.h"
#include "Core/Window.h"

#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Renderer/Image.h"

#include <GLFW/glfw3.h>
#include <nfd.hpp>
#include <nfd_glfw3.h>

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
		uint32_t width = 0, height = 0;
		Buffer pixels = Image::DecodeToRGBA8(path, width, height);
		if (!pixels.Data)
			return; // Image::DecodeToRGBA8 already logged the failure.

		// GLFW copies the pixel data during the call, so the buffer can be released
		// immediately afterwards.
		GLFWimage image;
		image.width = static_cast<int>(width);
		image.height = static_cast<int>(height);
		image.pixels = pixels.As<unsigned char>();
		glfwSetWindowIcon(m_Window, 1, &image);

		pixels.Release();
	}

    auto Window::SetCursorMode(const CursorMode mode) const -> void
	{
	    glfwSetInputMode(m_Window, GLFW_CURSOR, static_cast<int>(mode));
	}
}
