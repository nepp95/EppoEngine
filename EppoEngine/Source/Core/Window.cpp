#include "pch.h"
#include "Window.h"

#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	static void GLFWErrorCallback(int error, const char* description)
	{
		EPPO_ERROR("GLFW Error: ({}) {}", error, description);
	}

	Window::Window(const WindowSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Window::Window");

		int success = glfwInit();
		EPPO_ASSERT(success);

		#ifdef EPPO_DEBUG
			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
		#endif

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

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
		m_Window = glfwCreateWindow(m_Specification.Width, m_Specification.Height, m_Specification.Title.c_str(), nullptr, nullptr);
	}

	void Window::Init()
	{
		EPPO_PROFILE_FUNCTION("Window::Init");

		// Create renderer context
		m_Context = CreateRef<RendererContext>(m_Window);
		m_Context->Init();

		glfwSetWindowUserPointer(m_Window, &m_Callback);

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			WindowCloseEvent e;
			callback(e);
		});

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			WindowResizeEvent e(width, height);
			callback(e);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);

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
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			KeyTypedEvent e(keycode);
			callback(e);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			
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
		});
		
		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			MouseScrolledEvent e((float)xOffset, (float)yOffset);
			callback(e);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			EventCallbackFn& callback = *(EventCallbackFn*)glfwGetWindowUserPointer(window);
			MouseMovedEvent e((float)xPos, (float)yPos);
			callback(e);
		});
	}

	void Window::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Window::Shutdown");

		m_Context->Shutdown();

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void Window::ProcessEvents()
	{
		EPPO_PROFILE_FUNCTION("Window::ProcessEvents");
		EPPO_PROFILE_FN("CPU Update", "Process Events");

		glfwPollEvents();
	}
}
