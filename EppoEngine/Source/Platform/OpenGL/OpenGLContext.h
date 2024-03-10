#pragma once

#include "Renderer/RendererContext.h"

struct GLFWwindow;

namespace Eppo
{
    class OpenGLContext : public RendererContext
    {
    public:
        OpenGLContext(GLFWwindow* windowHandle);
        ~OpenGLContext() = default;

        void Init() override;
        void Shutdown() override;

        void BeginFrame() override;
        void PresentFrame() override;

        void OnResize() override;

        GLFWwindow* GetWindowHandle() const override { return m_WindowHandle; }

    private:
        GLFWwindow* m_WindowHandle = nullptr;
    };
}
