#include "pch.h"
#include "DescriptorWriter.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	void DescriptorWriter::WriteImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
	{
		EPPO_PROFILE_FUNCTION("DescriptorWriter::WriteImage");

		VkDescriptorImageInfo& info = m_ImageInfos.emplace_back();
		info.imageView = imageView;
		info.sampler = sampler;
		info.imageLayout = layout;

		VkWriteDescriptorSet& writeDescriptorSet = m_WriteDescriptors.emplace_back();
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.dstSet = VK_NULL_HANDLE;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.pImageInfo = &info;
	}

	void DescriptorWriter::WriteImages(uint32_t binding, const std::vector<VkDescriptorImageInfo>& imageInfos, VkDescriptorType type)
	{
		EPPO_PROFILE_FUNCTION("DescriptorWriter::WriteImages");

		if (imageInfos.empty())
			return;

		VkWriteDescriptorSet& writeDescriptorSet = m_WriteDescriptors.emplace_back();
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.dstSet = VK_NULL_HANDLE;
		writeDescriptorSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.pImageInfo = imageInfos.data();
	}

	void DescriptorWriter::WriteBuffer(uint32_t binding, VkBuffer buffer, uint32_t size, uint32_t offset, VkDescriptorType type)
	{
		EPPO_PROFILE_FUNCTION("DescriptorWriter::WriteBuffer");

		VkDescriptorBufferInfo& info = m_BufferInfos.emplace_back();
		info.buffer = buffer;
		info.range = size;
		info.offset = offset;

		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.dstSet = VK_NULL_HANDLE;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.pBufferInfo = &info;

		m_WriteDescriptors.emplace_back(writeDescriptorSet);
	}

	void DescriptorWriter::Clear()
	{
		m_WriteDescriptors.clear();
		m_BufferInfos.clear();
		m_ImageInfos.clear();
	}

	void DescriptorWriter::UpdateSet(VkDescriptorSet descriptorSet)
	{
		EPPO_PROFILE_FUNCTION("DescriptorWriter::UpdateSet");

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (VkWriteDescriptorSet& write : m_WriteDescriptors)
			write.dstSet = descriptorSet;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(m_WriteDescriptors.size()), m_WriteDescriptors.data(), 0, nullptr);
	}
}
