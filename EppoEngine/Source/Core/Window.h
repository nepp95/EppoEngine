#pragma once

#include "Event/Event.h"

struct GLFWwindow;

namespace Eppo
{
    enum class CursorMode
    {
        Normal = 0x00034001, // From glfw3.h
        Hidden = 0x00034002, // From glfw3.h
        Disabled = 0x00034003, // From glfw3.h
    };

    struct WindowSpecification
    {
        std::string Title = "EppoEngine";
        uint32_t Width = 1600;
        uint32_t Height = 900;
        bool Fullscreen = false;
        bool Decorated = true;
        bool EnableFileDialogs = true;
    };

    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        explicit Window(WindowSpecification specification);
        ~Window() = default;

        auto Shutdown() -> void;

        auto ProcessEvents() -> void;
        auto SetEventCallback(const EventCallbackFn& callback) { m_EventCallback = callback; }

        // Set the OS window/taskbar icon from an image file (RGBA). Best-effort:
        // logs and returns without changing the icon if the file cannot be loaded.
        auto SetIcon(const std::filesystem::path& path) -> void;

        auto SetCursorMode(CursorMode mode) const -> void;

        [[nodiscard]] auto GetNative() const -> GLFWwindow* { return m_Window; }
        [[nodiscard]] auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] auto GetHeight() const -> uint32_t { return m_Height; }
        [[nodiscard]] auto GetFramebufferSize() const -> std::pair<uint32_t, uint32_t>;
        [[nodiscard]] auto GetSpecification() const -> const WindowSpecification& { return m_Specification; }

    private:
        GLFWwindow* m_Window = nullptr;
        EventCallbackFn m_EventCallback;
        WindowSpecification m_Specification;

        uint32_t m_Width;
        uint32_t m_Height;
        bool m_FileDialogsInitialized = false;
    };
}
