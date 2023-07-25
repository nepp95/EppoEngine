#pragma once

#include "Renderer/RendererContext.h"

struct GLFWwindow;

namespace Eppo
{
	struct WindowSpecification
	{
		std::string Title;

		uint32_t Width = 1280;
		uint32_t Height = 720;

		uint32_t RefreshRate = 60.0f;
		
		// If this is set to true, glfw will override above information with information gathered from the primary monitor
		bool OverrideSpecification = true;
	};

	class Window
	{
	public:
		Window(const WindowSpecification& specification);

		void Init();
		void Shutdown();

		Ref<RendererContext> GetRendererContext() { return m_Context; }

	private:
		WindowSpecification m_Specification;
		GLFWwindow* m_Window;

		Ref<RendererContext> m_Context;
	};
}
