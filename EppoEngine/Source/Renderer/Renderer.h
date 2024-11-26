#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"

using VkDescriptorSet = struct VkDescriptorSet_T*;
using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;

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
		static void BeginRenderPass(Ref<CommandBuffer> commandBuffer, Ref<Pipeline> pipeline);
		static void EndRenderPass(Ref<CommandBuffer> commandBuffer);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);
		static VkDescriptorSet AllocateDescriptor(VkDescriptorSetLayout layout);
	};
}
