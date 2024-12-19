#include "pch.h"
#include "DescriptorLayoutBuilder.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	bool DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
	{
		if (other.Bindings.size() != Bindings.size())
			return false;

		for (size_t i = 0; i < Bindings.size(); i++)
		{
			if (other.Bindings[i].binding != Bindings[i].binding)
				return false;
			if (other.Bindings[i].descriptorType != Bindings[i].descriptorType)
				return false;
			if (other.Bindings[i].descriptorCount != Bindings[i].descriptorCount)
				return false;
			if (other.Bindings[i].stageFlags != Bindings[i].stageFlags)
				return false;
		}

		return true;
	}

	size_t DescriptorLayoutInfo::hash() const
	{
		size_t result = std::hash<size_t>()(Bindings.size());

		for (const auto& b : Bindings)
		{
			size_t bindingHash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
			result ^= std::hash<size_t>()(bindingHash);
		}

		return result;
	}

	void DescriptorLayoutBuilder::AddBinding(const uint32_t binding, const VkDescriptorType type, const uint32_t count)
	{
		VkDescriptorSetLayoutBinding& newBinding = m_CurrentLayoutInfo.Bindings.emplace_back();
		newBinding.binding = binding;
		newBinding.descriptorCount = count > 0 ? count : 1;
		newBinding.descriptorType = type;
	}

	void DescriptorLayoutBuilder::Clear()
	{
		m_CurrentLayoutInfo.Bindings.clear();
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Build(const VkShaderStageFlags shaderStageFlags, const VkDescriptorSetLayoutCreateFlags createFlags, const void* pNext)
	{
		EPPO_PROFILE_FUNCTION("DescriptorLayoutBuilder::Build");

		for (auto& binding : m_CurrentLayoutInfo.Bindings)
			binding.stageFlags |= shaderStageFlags;

		// Sort bindings so we always verify the hash correctly
		std::sort(m_CurrentLayoutInfo.Bindings.begin(), m_CurrentLayoutInfo.Bindings.end(), [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs)
		{
			return lhs.binding < rhs.binding;
		});

		// Check cache and return layout if we cached it
		const auto context = VulkanContext::Get();
		const VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorSetLayout layout;
		if (const auto it = m_DescriptorLayoutCache.find(m_CurrentLayoutInfo); it != m_DescriptorLayoutCache.end())
			layout = it->second;
		else
		{
			// Layout not cached, create new layout
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(m_CurrentLayoutInfo.Bindings.size());
			descriptorSetLayoutCreateInfo.pBindings = m_CurrentLayoutInfo.Bindings.data();
			descriptorSetLayoutCreateInfo.flags = createFlags;
			descriptorSetLayoutCreateInfo.pNext = pNext;

			VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &layout), "Failed to create descriptor set layout!")

			// Cache layout
			m_DescriptorLayoutCache[m_CurrentLayoutInfo] = layout;

			context->SubmitResourceFree([device, layout]()
			{
				EPPO_MEM_WARN("Releasing descriptor set layout {}", static_cast<void*>(layout));
				vkDestroyDescriptorSetLayout(device, layout, nullptr);
			});
		}
		
		Clear();

		return layout;
	}
}
