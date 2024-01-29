#pragma once

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
	// Descriptor abstraction from https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
	class DescriptorAllocator : public RefCounter
	{
	public:
		DescriptorAllocator() = default;
		~DescriptorAllocator() = default;

		void Shutdown();

		void ResetPools();
		bool Allocate(VkDescriptorSet* descriptorSet, VkDescriptorSetLayout layout);

	public:
		struct PoolSizes {
			std::vector<std::pair<VkDescriptorType, float>> Sizes = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER,					0.5f },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	4.0f },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				4.0f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				1.0f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		1.0f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		1.0f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			2.0f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			2.0f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,	1.0f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	1.0f },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			0.5f }
			};
		};

		VkDescriptorPool CreatePool(const PoolSizes& poolSizes, uint32_t count, VkDescriptorPoolCreateFlags flags = 0);

	private:
		VkDescriptorPool GrabPool();

	private:
		VkDescriptorPool m_CurrentPool = nullptr;
		PoolSizes m_DescriptorSizes;
		std::vector<VkDescriptorPool> m_UsedPools;
		std::vector<VkDescriptorPool> m_FreePools;
	};
}
