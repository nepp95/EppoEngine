#pragma once

struct GLFWwindow;

namespace Eppo
{
	enum class RendererAPI
	{
		OpenGL,
		Vulkan
	};

	class RendererContext : public RefCounter
	{
	public:
		virtual ~RendererContext() {}

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		static RendererAPI GetAPI() { return s_API; }
		static Ref<RendererContext> Get();
		static Ref<RendererContext> Create(void* windowHandle);

	private:
		static RendererAPI s_API;
	};
}
