#include "pch.h"
#include "Renderer.h"

#include "Renderer/RendererAPI.h"

namespace Eppo
{
	static RendererAPI* s_RendererAPI = nullptr;

	void Renderer::Init()
	{
		s_RendererAPI->Init();
	}

	void Renderer::Shutdown()
	{
		s_RendererAPI->Shutdown();
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return s_RendererAPI->GetCurrentFrameIndex();
	}

	void Renderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline)
	{
		s_RendererAPI->BeginRenderPass(renderCommandBuffer, pipeline);
	}

	void Renderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->EndRenderPass(renderCommandBuffer);
	}

	void Renderer::ExecuteRenderCommands()
	{
		s_RendererAPI->ExecuteRenderCommands();
	}

	void Renderer::SubmitCommand(RenderCommand command)
	{
		s_RendererAPI->SubmitCommand(command);
	}

	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_RendererAPI->GetShader(name);
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4& transform)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, uniformBufferSet, mesh, transform);
	}

	void RendererAPI::SetAPI(RendererAPIType type)
	{
		EPPO_ASSERT(type == RendererAPIType::Vulkan);
		s_CurrentAPI = type;
	}
}
