#pragma once

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
	class DescriptorAllocator
	{
	public:
		struct PoolSizeRatio
		{
			VkDescriptorType Type;
			float Ratio;
		};

		void Init(uint32_t initialSets, const std::vector<PoolSizeRatio>& poolSizeRatios);
		void ClearPools();
		void DestroyPools();

		void* Allocate(void* layout, const void* pNext = nullptr);

	private:
		VkDescriptorPool GetPool();
		[[nodiscard]] VkDescriptorPool CreatePool(uint32_t setCount) const;

	private:
		std::vector<PoolSizeRatio> m_PoolSizeRatios;
		std::vector<VkDescriptorPool> m_FullPools;
		std::vector<VkDescriptorPool> m_AvailablePools;

		uint32_t m_SetsPerPool = 0;
	};
}
