#include "pch.h"
#include "Window.h"

#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	static void GLFWErrorCallback(int error, const char* description)
	{
		EPPO_ERROR("GLFW Error: ({}) {}", error, description);
	}

	Window::Window(WindowSpecification specification)
		: m_Specification(std::move(specification))
	{
		const int success = glfwInit();
		EPPO_ASSERT(success)

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		#ifdef EPPO_DEBUG
			glfwSetErrorCallback(GLFWErrorCallback);
		#endif

		// Get primary monitor
		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

		if (m_Specification.OverrideSpecification)
		{
			m_Specification.Width = mode->width;
			m_Specification.Height = mode->height;
			m_Specification.RefreshRate = mode->refreshRate;
		}

		// Create window
		EPPO_INFO("Creating window '{}' ({}x{}@{}Hz)", m_Specification.Title, m_Specification.Width, m_Specification.Height, m_Specification.RefreshRate);
		glfwWindowHint(GLFW_POSITION_X, 25);
		glfwWindowHint(GLFW_POSITION_Y, 50);
		m_Window = glfwCreateWindow(static_cast<int>(m_Specification.Width), static_cast<int>(m_Specification.Height),
		                            m_Specification.Title.c_str(), nullptr, nullptr);
	}

	void Window::Init()
	{
		// Create renderer context
		m_Context = RendererContext::Create(m_Window);
		m_Context->Init();

		glfwSetWindowUserPointer(m_Window, &m_Callback);

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			WindowCloseEvent e;
			callback(e);
		});

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			WindowResizeEvent e(width, height);
			callback(e);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:
				{
					KeyPressedEvent e(static_cast<uint8_t>(key));
					callback(e);
					break;
				}

				case GLFW_RELEASE:
				{
					KeyReleasedEvent e(static_cast<uint8_t>(key));
					callback(e);
					break;
				}

				case GLFW_REPEAT:
				{
					KeyPressedEvent e(static_cast<uint8_t>(key), true);
					callback(e);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			KeyTypedEvent e(static_cast<uint8_t>(keycode));
			callback(e);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			
			switch (action)
			{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent e(static_cast<uint8_t>(button));
					callback(e);
					break;
				}

				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent e(static_cast<uint8_t>(button));
					callback(e);
					break;
				}
			}
		});
		
		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			MouseScrolledEvent e(static_cast<float>(xOffset), static_cast<float>(yOffset));
			callback(e);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			const EventCallbackFn& callback = *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));
			MouseMovedEvent e(static_cast<float>(xPos), static_cast<float>(yPos));
			callback(e);
		});
	}

	void Window::Shutdown() const
	{
		m_Context->Shutdown();

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void Window::ProcessEvents() const
	{
		glfwPollEvents();
	}

	void Window::SetWindowTitle(const std::string& name) const
	{
		glfwSetWindowTitle(m_Window, name.c_str());
	}
}
