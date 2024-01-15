#pragma once

#include "Event/Event.h"
#include "Renderer/RendererContext.h"

struct GLFWwindow;

namespace Eppo
{
	struct WindowSpecification
	{
		std::string Title;

		uint32_t Width = 1600;
		uint32_t Height = 900;

		uint32_t RefreshRate = 60;
		
		// If this is set to true, glfw will override above information with information gathered from the primary monitor
		bool OverrideSpecification = false;
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Window(const WindowSpecification& specification);

		void Init();
		void Shutdown();

		void ProcessEvents();
		void SetEventCallback(const EventCallbackFn& callback) { m_Callback = callback; }

		GLFWwindow* GetNativeWindow() const { return m_Window; }

		Ref<RendererContext> GetRendererContext() { return m_Context; }

	private:
		WindowSpecification m_Specification;
		GLFWwindow* m_Window;

		Ref<RendererContext> m_Context;

		EventCallbackFn m_Callback;
	};
}
