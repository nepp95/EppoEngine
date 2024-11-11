#pragma once

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

		static uint32_t GetCurrentFrameIndex();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);
		static void RegisterSingleUseDescriptor(VkDescriptorSet descriptorSet);
		static void ClearSingleUseDescriptors();

		// Render passes
		static void RT_BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline);
		static void RT_EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);
		static VkDescriptorSet AllocateDescriptor(VkDescriptorSetLayout layout);

		// Geometry
		static void RT_RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Mesh> mesh, const glm::mat4& transform);
	};
}
