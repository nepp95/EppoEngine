#include "pch.h"
#include "DescriptorBuilder.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	DescriptorBuilder::DescriptorBuilder(Ref<DescriptorAllocator> allocator, Ref<DescriptorLayoutCache> layoutCache)
		: m_Allocator(allocator), m_LayoutCache(layoutCache)
	{
		EPPO_PROFILE_FUNCTION("DescriptorBuilder::DescriptorBuilder");
	}

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		EPPO_PROFILE_FUNCTION("DescriptorBuilder::BindBuffer");

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

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		EPPO_PROFILE_FUNCTION("DescriptorBuilder::BindImage");

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

	bool DescriptorBuilder::Build(VkDescriptorSet& set)
	{
		EPPO_PROFILE_FUNCTION("DescriptorBuilder::Build");

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
		EPPO_PROFILE_FUNCTION("DescriptorBuilder::Build");

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
