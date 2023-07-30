#pragma once

#include "Renderer/Vma.h"

namespace Eppo
{
	class Allocator
	{
	public:
		Allocator() = default;
		~Allocator() = default;

		static void Init();
		static void Shutdown();

		static VmaAllocation AllocateBuffer(VkBuffer& buffer, VkBufferCreateInfo createInfo, VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO);
		static VmaAllocation AllocateImage(VkImage& image, VkImageCreateInfo createInfo, VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO);
		static void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
		static void DestroyImage(VkImage image, VmaAllocation allocation);

		static void* MapMemory(VmaAllocation allocation);
		static void UnmapMemory(VmaAllocation allocation);
	};
}
