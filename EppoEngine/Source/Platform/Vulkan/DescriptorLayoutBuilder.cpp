#include "pch.h"
#include "DescriptorLayoutBuilder.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		VkDescriptorSetLayoutBinding& newBinding = m_Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = count > 0 ? count : 1;
		newBinding.descriptorType = type;
	}

	void DescriptorLayoutBuilder::Clear()
	{
		m_Bindings.clear();
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkShaderStageFlags shaderStageFlags, VkDescriptorSetLayoutCreateFlags createFlags, void* pNext)
	{
		for (auto& binding : m_Bindings)
			binding.stageFlags |= shaderStageFlags;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(m_Bindings.size());
		descriptorSetLayoutCreateInfo.pBindings = m_Bindings.data();
		descriptorSetLayoutCreateInfo.flags = createFlags;
		descriptorSetLayoutCreateInfo.pNext = pNext;

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &layout), "Failed to create descriptor set layout!");

		return layout;
	}
}
