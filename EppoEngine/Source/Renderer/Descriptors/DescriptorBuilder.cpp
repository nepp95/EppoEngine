#include "pch.h"
#include "DescriptorBuilder.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	DescriptorBuilder::DescriptorBuilder(Ref<DescriptorAllocator> allocator, Ref<DescriptorLayoutCache> layoutCache)
		: m_Allocator(allocator), m_LayoutCache(layoutCache)
	{}

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		// Create the layout binding
		VkDescriptorSetLayoutBinding& newBinding = m_Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = 1;
		newBinding.descriptorType = type;
		newBinding.stageFlags = stageFlags;
		newBinding.pImmutableSamplers = nullptr;

		// Create write descriptor
		VkWriteDescriptorSet& writeDesc = m_WriteDescriptors.emplace_back();
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.descriptorCount = 1;
		writeDesc.descriptorType = type;
		writeDesc.dstBinding = binding;
		writeDesc.pBufferInfo = &bufferInfo;
		writeDesc.pImageInfo = nullptr;
		writeDesc.pTexelBufferView = nullptr;
		writeDesc.pNext = nullptr;

		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, const std::vector<VkDescriptorBufferInfo>& bufferInfos, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		size_t arraySize = bufferInfos.size();

		// Create the layout binding
		VkDescriptorSetLayoutBinding& newBinding = m_Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = arraySize;
		newBinding.descriptorType = type;
		newBinding.stageFlags = stageFlags;
		newBinding.pImmutableSamplers = nullptr;

		// Create write descriptor
		for (size_t i = 0; i < arraySize; i++)
		{
			VkWriteDescriptorSet& writeDesc = m_WriteDescriptors.emplace_back();
			writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDesc.descriptorCount = 1;
			writeDesc.descriptorType = type;
			writeDesc.dstBinding = binding;
			writeDesc.dstArrayElement = i;
			writeDesc.pBufferInfo = &bufferInfos[i];
			writeDesc.pImageInfo = nullptr;
			writeDesc.pTexelBufferView = nullptr;
			writeDesc.pNext = nullptr;
		}

		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		// Create the layout binding
		VkDescriptorSetLayoutBinding newBinding = m_Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = 1;
		newBinding.descriptorType = type;
		newBinding.stageFlags = stageFlags;
		newBinding.pImmutableSamplers = nullptr;

		// Create write descriptor
		VkWriteDescriptorSet writeDesc = m_WriteDescriptors.emplace_back();
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.descriptorCount = 1;
		writeDesc.descriptorType = type;
		writeDesc.dstBinding = binding;
		writeDesc.pBufferInfo = nullptr;
		writeDesc.pImageInfo = &imageInfo;
		writeDesc.pTexelBufferView = nullptr;
		writeDesc.pNext = nullptr;

		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, const std::vector<VkDescriptorImageInfo>& imageInfos, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		size_t arraySize = imageInfos.size();

		// Create the layout binding
		VkDescriptorSetLayoutBinding newBinding = m_Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = arraySize;
		newBinding.descriptorType = type;
		newBinding.stageFlags = stageFlags;
		newBinding.pImmutableSamplers = nullptr;

		// Create write descriptor
		for (size_t i = 0; i < arraySize; i++)
		{
			VkWriteDescriptorSet writeDesc = m_WriteDescriptors.emplace_back();
			writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDesc.descriptorCount = 1;
			writeDesc.descriptorType = type;
			writeDesc.dstBinding = binding;
			writeDesc.dstArrayElement = i;
			writeDesc.pBufferInfo = nullptr;
			writeDesc.pImageInfo = &imageInfos[i];
			writeDesc.pTexelBufferView = nullptr;
			writeDesc.pNext = nullptr;
		}

		return *this;
	}

	bool DescriptorBuilder::Build(VkDescriptorSet& set)
	{
		// Create descriptor set layout
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = m_Bindings.size();
		layoutInfo.pBindings = m_Bindings.data();
		layoutInfo.pNext = nullptr;

		VkDescriptorSetLayout layout = m_LayoutCache->CreateLayout(layoutInfo);

		if (!m_Allocator->Allocate(&set, layout))
			return false;

		for (VkWriteDescriptorSet& writeDesc : m_WriteDescriptors)
			writeDesc.dstSet = set;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, m_WriteDescriptors.size(), m_WriteDescriptors.data(), 0, nullptr);

		return true;
	}

	bool DescriptorBuilder::Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
	{
		// Create descriptor set layout
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = m_Bindings.size();
		layoutInfo.pBindings = m_Bindings.data();
		layoutInfo.pNext = nullptr;

		layout = m_LayoutCache->CreateLayout(layoutInfo);

		if (!m_Allocator->Allocate(&set, layout))
			return false;

		for (VkWriteDescriptorSet& writeDesc : m_WriteDescriptors)
			writeDesc.dstSet = set;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, m_WriteDescriptors.size(), m_WriteDescriptors.data(), 0, nullptr);
	
		return true;
	}
}
