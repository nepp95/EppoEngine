#pragma once

#include "Platform/Vulkan/Descriptor/DescriptorAllocator.h"
#include "Platform/Vulkan/Descriptor/DescriptorLayoutCache.h"
#include "Renderer/RendererAPI.h"
#include "Renderer/Buffer/UniformBuffer.h"

namespace Eppo
{
	// TODO: Do we still need all the const references with our new ref counter?
	class VulkanRenderer : public RendererAPI
	{
	public:
		VulkanRenderer() = default;
		~VulkanRenderer();

		void Init() override;
		void Shutdown() override;

		// Frame index
		uint32_t GetCurrentFrameIndex() const override;

		// Render pass
		void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline) override;
		void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) override;

		// Render commands
		void ExecuteRenderCommands() override;
		void SubmitCommand(RenderCommand command) override;

		// Shaders
		Ref<Shader> GetShader(const std::string& name) override;

		// Geometry
		void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4& transform) override;

		// Descriptor sets
		static Ref<DescriptorAllocator> GetDescriptorAllocator();
		static Ref<DescriptorLayoutCache> GetDescriptorLayoutCache();
		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);
	};
}
