#include "pch.h"
#include "Window.h"

#include "Events/ApplicationEvent.h"

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

		glfwPollEvents();
	}
}
