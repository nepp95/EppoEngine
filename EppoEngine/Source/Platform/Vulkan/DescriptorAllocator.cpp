#include "pch.h"
#include "DescriptorAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	void DescriptorAllocator::Init(const uint32_t initialSets, const std::vector<PoolSizeRatio>& poolSizeRatios)
	{
		m_PoolSizeRatios = poolSizeRatios;

		VkDescriptorPool newPool = CreatePool(initialSets);
		m_SetsPerPool = initialSets * 1.5;

		m_AvailablePools.emplace_back(newPool);
	}

	void DescriptorAllocator::ClearPools()
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::ClearPools");

		const auto context = VulkanContext::Get();
		const VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (const auto& pool : m_AvailablePools)
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

		const auto context = VulkanContext::Get();
		const VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (const auto& pool : m_AvailablePools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		for (const auto& pool : m_FullPools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		m_FullPools.clear();
	}

	void* DescriptorAllocator::Allocate(void* layout, const void* pNext)
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::Allocate");

		const auto context = VulkanContext::Get();
		const VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorPool pool = GetPool();
		const auto vkLayout = static_cast<VkDescriptorSetLayout>(layout);

		VkDescriptorSetAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = pool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &vkLayout;
		allocateInfo.pNext = pNext;

		VkDescriptorSet descriptorSet;

		if (const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);
			result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
		{
			EPPO_WARN("Descriptor pool is full, trying again with next available pool!");

			m_FullPools.emplace_back(pool);

			pool = GetPool();
			allocateInfo.descriptorPool = pool;

			VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet), "Failed to allocate descriptor set!")
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
			m_SetsPerPool = std::min<uint32_t>(m_SetsPerPool, 4092);
		}

		return newPool;
	}

	VkDescriptorPool DescriptorAllocator::CreatePool(const uint32_t setCount) const
	{
		EPPO_PROFILE_FUNCTION("DescriptorAllocator::CreatePool");

		std::vector<VkDescriptorPoolSize> poolSizes;
		for (const auto& [type, ratio] : m_PoolSizeRatios)
		{
			VkDescriptorPoolSize& poolSize = poolSizes.emplace_back();
			poolSize.type = type;
			poolSize.descriptorCount = ratio * setCount;
		}

		VkDescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.flags = 0;
		poolCreateInfo.maxSets = setCount;
		poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolCreateInfo.pPoolSizes = poolSizes.data();

		const auto context = VulkanContext::Get();
		const VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorPool newPool;
		VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &newPool), "Failed to create descriptor pool!")

		return newPool;
	}
}
