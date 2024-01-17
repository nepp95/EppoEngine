#include "pch.h"
#include "VulkanAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	struct VmaData
	{
		VmaAllocator Allocator;

		uint64_t MemoryAllocated = 0;
		uint64_t MemoryFreed = 0;

		uint64_t MemoryUsed()
		{
			return MemoryAllocated - MemoryFreed;
		}
	};

	static VmaData* s_Data;

	void VulkanAllocator::Init()
	{
		EPPO_PROFILE_FUNCTION("Allocator::Init");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();

		s_Data = new VmaData();

		VmaAllocatorCreateInfo createInfo{};
		createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		createInfo.instance = context->GetVulkanInstance();
		createInfo.physicalDevice = context->GetPhysicalDevice()->GetNativeDevice();
		createInfo.device = context->GetLogicalDevice()->GetNativeDevice();

		VK_CHECK(vmaCreateAllocator(&createInfo, &s_Data->Allocator), "Failed to create allocator!");

		context->SubmitResourceFree([]()
		{
			VulkanAllocator::Shutdown();
		});
	}

	void VulkanAllocator::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Allocator::Shutdown");

		vmaDestroyAllocator(s_Data->Allocator);
		delete s_Data;
	}

	VmaAllocation VulkanAllocator::AllocateBuffer(VkBuffer& buffer, const VkBufferCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		EPPO_PROFILE_FUNCTION("Allocator::AllocateBuffer");

		VmaAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateBuffer(s_Data->Allocator, &createInfo, &allocCreateInfo, &buffer, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	VmaAllocation VulkanAllocator::AllocateImage(VkImage& image, const VkImageCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		EPPO_PROFILE_FUNCTION("Allocator::AllocateImage");

		VmaAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateImage(s_Data->Allocator, &createInfo, &allocCreateInfo, &image, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	void VulkanAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
	{
		EPPO_PROFILE_FUNCTION("Allocator::DestroyBuffer");

		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyBuffer(s_Data->Allocator, buffer, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	void VulkanAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
	{
		EPPO_PROFILE_FUNCTION("Allocator::DestroyImage");

		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyImage(s_Data->Allocator, image, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	void* VulkanAllocator::MapMemory(VmaAllocation allocation)
	{
		EPPO_PROFILE_FUNCTION("Allocator::MapMemory");

		void* data;
		vmaMapMemory(s_Data->Allocator, allocation, &data);
		return data;
	}

	void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
	{
		EPPO_PROFILE_FUNCTION("Allocator::UnmapMemory");

		vmaUnmapMemory(s_Data->Allocator, allocation);
	}
}
