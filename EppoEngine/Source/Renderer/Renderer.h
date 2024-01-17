#pragma once

#include "Renderer/Buffer/UniformBuffer.h"
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
		virtual ~Renderer() {}

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		// Frame index
		virtual uint32_t GetCurrentFrameIndex() const = 0;

		// Render pass
		virtual void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline) = 0;
		virtual void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) = 0;

		// Render commands
		virtual void ExecuteRenderCommands() = 0;
		virtual void SubmitCommand(RenderCommand command) = 0;

		// Shaders
		virtual Ref<Shader> GetShader(const std::string& name) = 0;

		// Geometry
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBuffer> environmentUB, Ref<UniformBuffer> cameraUB, Ref<Mesh> mesh, const glm::mat4& transform) = 0;
	
		// Create me
		static Ref<Renderer> Create();
	};
}
