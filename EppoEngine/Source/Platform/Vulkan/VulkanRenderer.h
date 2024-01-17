#pragma once

#include "Platform/Vulkan/Descriptor/DescriptorAllocator.h"
#include "Platform/Vulkan/Descriptor/DescriptorLayoutCache.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	// TODO: Do we still need all the const references with our new ref counter?
	class VulkanRenderer : public Renderer
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
		void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBuffer> environmentUB, Ref<UniformBuffer> cameraUB, Ref<Mesh> mesh, const glm::mat4& transform) override;

		// Descriptor sets
		Ref<DescriptorAllocator> GetDescriptorAllocator() const { return m_DescriptorAllocator; }
		Ref<DescriptorLayoutCache> GetDescriptorLayoutCache() const { return m_DescriptorLayoutCache; }
		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo) const;

	private:
		Scope<RenderCommandQueue> m_CommandQueue;
		Ref<RenderCommandBuffer> m_CommandBuffer;

		Ref<DescriptorAllocator> m_DescriptorAllocator;
		Ref<DescriptorLayoutCache> m_DescriptorLayoutCache;
		std::vector<VkDescriptorPool> m_DescriptorPools;

		Scope<ShaderLibrary> m_ShaderLibrary;
	};
}
