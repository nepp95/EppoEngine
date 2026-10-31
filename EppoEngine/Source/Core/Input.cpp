#include "pch.h"
#include "Core/Input.h"

#include "Core/Application.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
    bool Input::s_ViewportInputEnabled = true;
    bool Input::s_HasViewportBounds = false;
    glm::vec2 Input::s_ViewportMin = glm::vec2(0.0f);
    glm::vec2 Input::s_ViewportMax = glm::vec2(0.0f);

    auto Input::SetViewportInputEnabled(bool enabled) -> void
    {
        s_ViewportInputEnabled = enabled;
    }

    auto Input::IsViewportInputEnabled() -> bool
    {
        return s_ViewportInputEnabled;
    }

    auto Input::IsKeyPressed(KeyCode key) -> bool
    {
        return s_ViewportInputEnabled && IsKeyPressedRaw(key);
    }

    auto Input::IsKeyPressedRaw(KeyCode key) -> bool
    {
        GLFWwindow* window = Application::Get().GetWindow()->GetNative();
        return glfwGetKey(window, key) == GLFW_PRESS;
    }

    auto Input::IsMouseButtonPressed(MouseCode button) -> bool
    {
        if (!s_ViewportInputEnabled)
            return false;

        GLFWwindow* window = Application::Get().GetWindow()->GetNative();
        return glfwGetMouseButton(window, button) == GLFW_PRESS;
    }

    auto Input::GetMousePosition() -> glm::vec2
    {
        GLFWwindow* window = Application::Get().GetWindow()->GetNative();

        double xPos, yPos;
        glfwGetCursorPos(window, &xPos, &yPos);

        return { static_cast<float>(xPos), static_cast<float>(yPos) };
    }

    auto Input::GetMouseX() -> float
    {
        return GetMousePosition().x;
    }

    auto Input::GetMouseY() -> float
    {
        return GetMousePosition().y;
    }

    auto Input::SetViewportBounds(const glm::vec2& min, const glm::vec2& max) -> void
    {
        s_HasViewportBounds = true;
        s_ViewportMin = min;
        s_ViewportMax = max;
    }

    auto Input::GetViewportMin() -> glm::vec2
    {
        return s_HasViewportBounds ? s_ViewportMin : glm::vec2(0.0f);
    }

    auto Input::GetViewportSize() -> glm::vec2
    {
        if (s_HasViewportBounds)
            return s_ViewportMax - s_ViewportMin;

        // Window size, not framebuffer size: it is the space glfwGetCursorPos reports in.
        GLFWwindow* window = Application::Get().GetWindow()->GetNative();

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        return { static_cast<float>(width), static_cast<float>(height) };
    }

    auto Input::GetViewportMousePosition() -> glm::vec2
    {
        return GetMousePosition() - GetViewportMin();
    }

    auto Input::SetCursorMode(const CursorMode mode) -> void
    {
        Application::Get().GetWindow()->SetCursorMode(mode);
    }

    auto Input::GetCursorMode() -> CursorMode
    {
        GLFWwindow* window = Application::Get().GetWindow()->GetNative();
        return static_cast<CursorMode>(glfwGetInputMode(window, GLFW_CURSOR));
    }
}
