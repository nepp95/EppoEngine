#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"

typedef struct VkDescriptorSet_T* VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		static uint32_t GetCurrentFrameIndex();

		// Render queue commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Render passes
		static void RT_BeginRenderPass(Ref<CommandBuffer> commandBuffer, Ref<Pipeline> pipeline);
		static void RT_EndRenderPass(Ref<CommandBuffer> commandBuffer);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);
		static VkDescriptorSet AllocateDescriptor(VkDescriptorSetLayout layout);
	};
}
