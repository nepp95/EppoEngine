#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
	class DescriptorWriter
	{
	public:
		void WriteImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
		void WriteImages(uint32_t binding, const std::vector<VkDescriptorImageInfo>& imageInfos, VkDescriptorType type);
		void WriteBuffer(uint32_t binding, VkBuffer buffer, uint32_t size, uint32_t offset, VkDescriptorType type);

		void Clear();
		void UpdateSet(VkDescriptorSet descriptorSet);

	private:
		std::deque<VkDescriptorImageInfo> m_ImageInfos;
		std::deque<VkDescriptorBufferInfo> m_BufferInfos;
		std::vector<VkWriteDescriptorSet> m_WriteDescriptors;
	};
}
