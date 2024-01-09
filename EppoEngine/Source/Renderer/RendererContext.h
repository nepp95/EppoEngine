#pragma once

struct GLFWwindow;

namespace Eppo
{
	enum class RendererAPI
	{
		OpenGL,
		Vulkan
	};

	class RendererContext
	{
	public:
		virtual ~RendererContext() {}

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		void WaitIdle();

		GLFWwindow* GetWindowHandle() { return m_WindowHandle; }

		TracyVkCtx GetCurrentProfilerContext() { return m_TracyContexts[m_Swapchain->GetCurrentImageIndex()]; }

		static RendererAPI GetAPI() { return s_API; }
		static Scope<RendererContext> Create(void* windowHandle);

	private:
		static RendererAPI s_API;

		GLFWwindow* m_WindowHandle = nullptr;

		// Tracy profiler context
		std::vector<TracyVkCtx> m_TracyContexts;
	};
}
