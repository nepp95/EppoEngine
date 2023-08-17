#include "pch.h"
#include "DescriptorAllocator.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	void DescriptorAllocator::Shutdown()
	{
		ResetPools();

		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (auto pool : m_FreePools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		for (auto pool : m_UsedPools)
			vkDestroyDescriptorPool(device, pool, nullptr);
	}

	void DescriptorAllocator::ResetPools()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		for (auto descriptorPool : m_UsedPools)
		{
			vkResetDescriptorPool(device, descriptorPool, 0);
			m_FreePools.push_back(descriptorPool);
		}

		m_UsedPools.clear();
		m_CurrentPool = nullptr;
	}

	bool DescriptorAllocator::Allocate(VkDescriptorSet* descriptorSet, VkDescriptorSetLayout layout)
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		if (m_CurrentPool == nullptr)
		{
			m_CurrentPool = GrabPool();
			m_UsedPools.push_back(m_CurrentPool);
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_CurrentPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;
		allocInfo.pNext = nullptr;

		VkResult result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSet);
		bool reallocate = false;

		switch (result)
		{
			case VK_SUCCESS: return true;
			case VK_ERROR_FRAGMENTED_POOL:
			case VK_ERROR_OUT_OF_POOL_MEMORY:
				reallocate = true;
				break;
			default:
				return false;
		}

		if (reallocate)
		{
			m_CurrentPool = GrabPool();
			m_UsedPools.push_back(m_CurrentPool);

			result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSet);
			if (result == VK_SUCCESS)
				return true;
		}

		return false;
	}

	VkDescriptorPool DescriptorAllocator::CreatePool(const PoolSizes& poolSizes, uint32_t count, VkDescriptorPoolCreateFlags flags)
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		std::vector<VkDescriptorPoolSize> sizes;

		sizes.reserve(poolSizes.Sizes.size());
		for (auto size : poolSizes.Sizes)
			sizes.push_back({ size.first, (uint32_t)(size.second * count) });

		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = count;
		createInfo.poolSizeCount = sizes.size();
		createInfo.pPoolSizes = sizes.data();
		createInfo.flags = flags;

		VkDescriptorPool descriptorPool;
		VK_CHECK(vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool), "Failed to create descriptor pool!");

		return descriptorPool;
	}

	VkDescriptorPool DescriptorAllocator::GrabPool()
	{
		if (m_FreePools.empty())
			return CreatePool(m_DescriptorSizes, 1000);

		VkDescriptorPool descriptorPool = m_FreePools.back();
		m_FreePools.pop_back();

		return descriptorPool;
	}
}
