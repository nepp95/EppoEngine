#pragma once

#include "Renderer/Descriptors/DescriptorAllocator.h"
#include "Renderer/Descriptors/DescriptorLayoutCache.h"

namespace Eppo
{
	// Descriptor abstraction from https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
	class DescriptorBuilder
	{
	public:
		DescriptorBuilder(Ref<DescriptorAllocator> allocator, Ref<DescriptorLayoutCache> layoutCache);

		DescriptorBuilder& BindBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& BindBuffer(uint32_t binding, const std::vector<VkDescriptorBufferInfo>& bufferInfos, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& BindImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& BindImage(uint32_t binding, const std::vector<VkDescriptorImageInfo>& imageInfos, VkDescriptorType type, VkShaderStageFlags stageFlags);

		bool Build(VkDescriptorSet& set);
		bool Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);

	private:
		std::vector<VkWriteDescriptorSet> m_WriteDescriptors;
		std::vector<VkDescriptorSetLayoutBinding> m_Bindings;

		Ref<DescriptorLayoutCache> m_LayoutCache;
		Ref<DescriptorAllocator> m_Allocator;
	};
}
