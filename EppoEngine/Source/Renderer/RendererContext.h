#pragma once

#include "Renderer/Renderer.h"

struct GLFWwindow;

namespace Eppo
{
	enum class RendererAPI : uint8_t
	{
		Vulkan = 1
	};

	class RendererContext
	{
	public:
		virtual ~RendererContext() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		[[nodiscard]] virtual uint32_t GetCurrentFrameIndex() const = 0;
		virtual void BeginFrame() = 0;
		virtual void PresentFrame() = 0;
		virtual void WaitIdle() = 0;

		[[nodiscard]] virtual Ref<Renderer> GetRenderer() const = 0;

		virtual GLFWwindow* GetWindowHandle() = 0;

		static RendererAPI GetAPI() { return s_API; }
		static Ref<RendererContext> Get();
		static Ref<RendererContext> Create(GLFWwindow* windowHandle);

	private:
		static RendererAPI s_API;
	};
}
