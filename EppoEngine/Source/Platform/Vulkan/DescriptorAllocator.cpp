#include "pch.h"
#include "DescriptorAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	void DescriptorAllocator::Init(uint32_t initialSets, const std::vector<PoolSizeRatio>& poolSizeRatios)
	{
		m_PoolSizeRatios = poolSizeRatios;

		VkDescriptorPool newPool = CreatePool(initialSets);
		m_SetsPerPool = initialSets * 1.5;

		m_AvailablePools.emplace_back(newPool);
	}

	void DescriptorAllocator::ClearPools()
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::ClearPools");

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (auto& pool : m_AvailablePools)
			vkResetDescriptorPool(device, pool, 0);

		for (auto& pool : m_FullPools)
		{
			vkResetDescriptorPool(device, pool, 0);
			m_AvailablePools.emplace_back(pool);
		}

		m_FullPools.clear();
	}

	void DescriptorAllocator::DestroyPools()
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::DestroyPools");

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (auto& pool : m_AvailablePools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		for (auto& pool : m_FullPools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		m_FullPools.clear();
	}

	VkDescriptorSet DescriptorAllocator::Allocate(VkDescriptorSetLayout layout, void* pNext)
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::Allocate");

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorPool pool = GetPool();

		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = pool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &layout;
		allocateInfo.pNext = pNext;

		VkDescriptorSet descriptorSet;
		VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);

		if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
		{
			EPPO_WARN("Descriptor pool is full, trying again with next available pool!");

			m_FullPools.emplace_back(pool);

			pool = GetPool();
			allocateInfo.descriptorPool = pool;

			VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet), "Failed to allocate descriptor set!");
		}

		m_AvailablePools.emplace_back(pool);

		return descriptorSet;
	}

	VkDescriptorPool DescriptorAllocator::GetPool()
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::GetPool");

		VkDescriptorPool newPool;
		if (!m_AvailablePools.empty())
		{
			// Get available pool
			newPool = m_AvailablePools.back();
			m_AvailablePools.pop_back();
		}
		else
		{
			// Create new pool
			newPool = CreatePool(m_SetsPerPool);

			m_SetsPerPool *= 1.5;
			if (m_SetsPerPool > 4092)
				m_SetsPerPool = 4092;
		}

		return newPool;
	}

	VkDescriptorPool DescriptorAllocator::CreatePool(uint32_t setCount)
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::CreatePool");

		std::vector<VkDescriptorPoolSize> poolSizes;
		for (const auto& ratio : m_PoolSizeRatios)
		{
			VkDescriptorPoolSize& poolSize = poolSizes.emplace_back();
			poolSize.type = ratio.Type;
			poolSize.descriptorCount = ratio.Ratio * setCount;
		}

		VkDescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.flags = 0;
		poolCreateInfo.maxSets = setCount;
		poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolCreateInfo.pPoolSizes = poolSizes.data();

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorPool newPool;
		VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &newPool), "Failed to create descriptor pool!");

		return newPool;
	}
}
