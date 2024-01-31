#include "pch.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLRenderer.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Renderer/RendererAPI.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	static RendererAPI* s_RendererAPI = nullptr;

	struct RendererData
	{
		Scope<ShaderLibrary> ShaderLibrary;
	};

	static RendererData* s_Data = nullptr;

	static RendererAPI* InitAPI()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return new OpenGLRenderer();
			}

			case RendererAPIType::Vulkan:
			{
				return new VulkanRenderer();
			}
		}

		EPPO_ASSERT(false);
		return nullptr;
	}

	void Renderer::Init()
	{
		s_RendererAPI = InitAPI();
		EPPO_ASSERT(s_RendererAPI);

		s_RendererAPI->Init();

		s_Data = new RendererData();

		// Load shaders
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();
		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/shadow.glsl");
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
}
