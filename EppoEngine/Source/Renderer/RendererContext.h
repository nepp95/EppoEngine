#pragma once

#include "Debug/Profiler.h"

#include <deque>
#include <functional>

struct GLFWwindow;

namespace Eppo
{
	class RendererContext
	{
	public:
		RendererContext(GLFWwindow* windowHandle);
		~RendererContext() = default;

		void Init();
		void Shutdown();

		GLFWwindow* GetWindowHandle() { return m_WindowHandle; }

		static Ref<RendererContext> Get();

	private:
		GLFWwindow* m_WindowHandle = nullptr;
	};
}
