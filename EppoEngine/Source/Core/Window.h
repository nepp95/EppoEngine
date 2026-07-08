#pragma once

#include "Event/Event.h"

#include <filesystem>

struct GLFWwindow;

namespace Eppo
{
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Window(uint32_t width, uint32_t height);
		~Window() = default;

		auto Shutdown() -> void;

		auto ProcessEvents() -> void;
		auto SetEventCallback(const EventCallbackFn& callback) { m_EventCallback = callback; }

		// Set the OS window/taskbar icon from an image file (RGBA). Best-effort:
		// logs and returns without changing the icon if the file cannot be loaded.
		auto SetIcon(const std::filesystem::path& path) -> void;

		[[nodiscard]] auto GetNative() const -> GLFWwindow* { return m_Window; }
		[[nodiscard]] auto GetWidth() const -> uint32_t { return m_Width; }
		[[nodiscard]] auto GetHeight() const -> uint32_t { return m_Height; }

	private:
		GLFWwindow* m_Window = nullptr;
		EventCallbackFn m_EventCallback;

		uint32_t m_Width;
		uint32_t m_Height;
	};
}