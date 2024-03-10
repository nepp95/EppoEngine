#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Buffer/UniformBufferSet.h"
#include "Renderer/Pipeline.h"

namespace Eppo
{
	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(const PipelineSpecification& specification);
		~VulkanPipeline();

		void UpdateUniforms(Ref<UniformBufferSet> uniformBufferSet) override;

		void UpdateDescriptors(uint32_t frameIndex, Ref<UniformBufferSet> uniformBufferSet);

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) { return m_DescriptorSets.at(frameIndex); }

	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;

		std::vector<VkDescriptorPool> m_DescriptorPools;
		std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> m_DescriptorSets; // frame > set
	};
}
