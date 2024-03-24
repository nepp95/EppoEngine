#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Eppo
{
	static void DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam)
	{
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH:
			{
				EPPO_ERROR(message);
				return;
			}

			case GL_DEBUG_SEVERITY_MEDIUM:
			{
				EPPO_WARN(message);
				return;
			}

			case GL_DEBUG_SEVERITY_LOW:
			{
				EPPO_INFO(message);
				return;
			}

			case GL_DEBUG_SEVERITY_NOTIFICATION:
			{
				EPPO_TRACE(message);
				return;
			}
		}
	}

	RendererContext::RendererContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EPPO_PROFILE_FUNCTION("RendererContext::RendererContext");

		EPPO_ASSERT(windowHandle);
	}

	void RendererContext::Init()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Init");

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		EPPO_ASSERT(status);

		EPPO_INFO("GPU Info:");
		EPPO_INFO("\tVendor: {}", glGetString(GL_VENDOR));
		EPPO_INFO("\tModel: {}", glGetString(GL_RENDERER));
		EPPO_INFO("\tVersion: {}", glGetString(GL_VERSION));

		EPPO_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5));

		#ifdef EPPO_DEBUG
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(DebugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		#endif

		// TODO: enable depth testing and blending here
	}

	void RendererContext::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Shutdown");
	}

	Ref<RendererContext> RendererContext::Get()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Get");

		return Application::Get().GetWindow().GetRendererContext();
	}
}
