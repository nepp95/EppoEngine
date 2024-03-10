#pragma once

#include "Renderer/Buffer/UniformBufferSet.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Frame index
		static uint32_t GetCurrentFrameIndex();

		// Render pass
		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline);
		static void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Geometry
		static void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4& transform);
	};
}
