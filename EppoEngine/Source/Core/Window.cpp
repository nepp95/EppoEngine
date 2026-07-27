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

    Window::Window(WindowSpecification specification)
        : m_Specification(std::move(specification)), m_Width(m_Specification.Width), m_Height(m_Specification.Height)
    {
        // Setup glfw
        int success = glfwInit();
        EP_ASSERT(success, "GLFW failed to initialize!");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, m_Specification.Decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
        glfwSetErrorCallback(GLFWErrorCallback);

        GLFWmonitor* monitor = nullptr;
        if (m_Specification.Fullscreen)
        {
            monitor = glfwGetPrimaryMonitor();
            if (!monitor)
            {
                glfwTerminate();
                EP_ASSERT(false, "No primary monitor is available for fullscreen mode.");
            }

            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode)
            {
                glfwTerminate();
                EP_ASSERT(false, "The primary monitor did not provide a video mode.");
            }

            m_Width = static_cast<uint32_t>(mode->width);
            m_Height = static_cast<uint32_t>(mode->height);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        }

        // Create window
        Log::Info("Creating window of size {}x{}", m_Width, m_Height);
        m_Window = glfwCreateWindow(static_cast<int>(m_Width), static_cast<int>(m_Height), m_Specification.Title.c_str(), monitor, nullptr);
        if (!m_Window)
        {
            glfwTerminate();
            EP_ASSERT(false, "GLFW failed to create the application window.");
        }

        // Init file dialogs
        if (m_Specification.EnableFileDialogs)
        {
            success = NFD::Init();
            if (success != NFD_OKAY)
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
                glfwTerminate();
                EP_ASSERT(false, "Native file dialogs failed to initialize.");
            }
            m_FileDialogsInitialized = true;
        }

        // Set event callbacks
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowCloseCallback(
            m_Window,
            [](GLFWwindow* window) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                WindowCloseEvent e;
                self->m_EventCallback(e);
            }
        );

        glfwSetWindowSizeCallback(
            m_Window,
            [](GLFWwindow* window, const int width, const int height) -> void
            {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                self->m_Width = static_cast<uint32_t>(width);
                self->m_Height = static_cast<uint32_t>(height);
                WindowResizeEvent e(width, height);
                self->m_EventCallback(e);
            }
        );

        glfwSetKeyCallback(
            m_Window,
            [](GLFWwindow* window, const int key, const int scancode, const int action, const int mods) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));

                switch (action)
                {
                    case GLFW_PRESS:
                    {
                        KeyPressedEvent e(key);
                        self->m_EventCallback(e);
                        break;
                    }

                    case GLFW_RELEASE:
                    {
                        KeyReleasedEvent e(key);
                        self->m_EventCallback(e);
                        break;
                    }

                    case GLFW_REPEAT:
                    {
                        KeyPressedEvent e(key, true);
                        self->m_EventCallback(e);
                        break;
                    }
                }
            }
        );

        glfwSetCharCallback(
            m_Window,
            [](GLFWwindow* window, const unsigned int key) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                KeyTypedEvent e(key);
                self->m_EventCallback(e);
            }
        );

        glfwSetMouseButtonCallback(
            m_Window,
            [](GLFWwindow* window, int button, int action, int mods) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));

                switch (action)
                {
                    case GLFW_PRESS:
                    {
                        MouseButtonPressedEvent e(button);
                        self->m_EventCallback(e);
                        break;
                    }

                    case GLFW_RELEASE:
                    {
                        MouseButtonReleasedEvent e(button);
                        self->m_EventCallback(e);
                        break;
                    }
                }
            }
        );

        glfwSetScrollCallback(
            m_Window,
            [](GLFWwindow* window, double xOffset, double yOffset) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                MouseScrolledEvent e(static_cast<float>(xOffset), static_cast<float>(yOffset));
                self->m_EventCallback(e);
            }
        );

        glfwSetCursorPosCallback(
            m_Window,
            [](GLFWwindow* window, double xPos, double yPos) -> void
            {
                const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                MouseMovedEvent e(static_cast<float>(xPos), static_cast<float>(yPos));
                self->m_EventCallback(e);
            }
        );
    }

    auto Window::Shutdown() -> void
    {
        if (m_FileDialogsInitialized)
            NFD::Quit();
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    auto Window::GetFramebufferSize() const -> std::pair<uint32_t, uint32_t>
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_Window, &width, &height);
        return { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) };
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
