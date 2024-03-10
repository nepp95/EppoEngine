#pragma once

#include "Platform/Vulkan/Vma.h"

namespace Eppo
{
	class VulkanAllocator
	{
	public:
		VulkanAllocator() = default;
		~VulkanAllocator() = default;

		static void Init();
		static void Shutdown();

		static VmaAllocation AllocateBuffer(VkBuffer& buffer, const VkBufferCreateInfo& createInfo, VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO);
		static VmaAllocation AllocateImage(VkImage& image, const VkImageCreateInfo& createInfo, VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO);
		static void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
		static void DestroyImage(VkImage image, VmaAllocation allocation);

		static void* MapMemory(VmaAllocation allocation);
		static void UnmapMemory(VmaAllocation allocation);

	private:
		// TODO: Make a non static class
	};
}
