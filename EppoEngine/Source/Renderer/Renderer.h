#pragma once

#include "Renderer/Pipeline.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/CommandQueue.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	class Renderer
	{
	public:
		virtual ~Renderer() = default;

		struct RenderStatistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Submeshes = 0;
		};

		virtual void Shutdown() = 0;

		// Render queue commands
		virtual void ExecuteRenderCommands() = 0;
		virtual void SubmitCommand(RenderCommand command) = 0;

		// Render passes
		virtual void BeginRenderPass(const Ref<CommandBuffer>& commandBuffer, const Ref<Pipeline>& pipeline) = 0;
		virtual void EndRenderPass(const Ref<CommandBuffer>& commandBuffer) = 0;

		// Shaders
		virtual Ref<Shader> GetShader(const std::string& name) = 0;
		virtual void* AllocateDescriptor(void* layout) = 0;

		static Ref<Renderer> Create();
	};
}
