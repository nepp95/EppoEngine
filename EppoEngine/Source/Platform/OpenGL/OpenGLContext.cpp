#include "pch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Eppo
{
    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle)
    {
        EPPO_PROFILE_FUNCTION("OpenGLContext::OpenGLContext");

        EPPO_ASSERT(m_WindowHandle);
    }

    void OpenGLContext::Init()
    {
        EPPO_PROFILE_FUNCTION("OpenGLContext::Init");

        glfwMakeContextCurrent(m_WindowHandle);

        // Initialize glad
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        EPPO_ASSERT(status);
    }

    void OpenGLContext::Shutdown()
    {

    }

    void OpenGLContext::BeginFrame()
    {
		glClear(GL_COLOR_BUFFER_BIT);
    }

    void OpenGLContext::PresentFrame()
    {
		glfwSwapBuffers(m_WindowHandle);
    }

    void OpenGLContext::OnResize()
    {

    }
}
