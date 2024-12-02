#include "pch.h"
#include "VulkanAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	struct AllocatorData
	{
		VmaAllocator Allocator;

		uint64_t MemoryAllocated = 0;
		uint64_t MemoryFreed = 0;

		uint64_t MemoryUsed() const
		{
			return MemoryAllocated - MemoryFreed;
		}
	};

	static AllocatorData* s_Data;

	void VulkanAllocator::Init()
	{
		Ref<VulkanContext> context = VulkanContext::Get();

		s_Data = new AllocatorData();

		VmaVulkanFunctions vulkanFunctions{};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo createInfo{};
		createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		createInfo.instance = VulkanContext::GetVulkanInstance();
		createInfo.physicalDevice = context->GetPhysicalDevice()->GetNativeDevice();
		createInfo.device = context->GetLogicalDevice()->GetNativeDevice();
		createInfo.pVulkanFunctions = &vulkanFunctions;

		VK_CHECK(vmaCreateAllocator(&createInfo, &s_Data->Allocator), "Failed to create vma allocator!");

		context->SubmitResourceFree([]()
		{
			EPPO_WARN("Allocator::Shutdown");
			VulkanAllocator::Shutdown();
		});
	}

	void VulkanAllocator::Shutdown()
	{
		if (s_Data->MemoryUsed() > 0)
			EPPO_WARN("Still {} memory in use by VMA", s_Data->MemoryUsed());

		vmaDestroyAllocator(s_Data->Allocator);
		delete s_Data;
	}

	VmaAllocation VulkanAllocator::AllocateBuffer(VkBuffer& buffer, const VkBufferCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateBuffer(s_Data->Allocator, &createInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	void VulkanAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
	{
		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyBuffer(s_Data->Allocator, buffer, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	VmaAllocation VulkanAllocator::AllocateImage(VkImage& image, const VkImageCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo  allocationCreateInfo{};
		allocationCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateImage(s_Data->Allocator, &createInfo, &allocationCreateInfo, &image, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	void VulkanAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
	{
		VmaAllocationInfo  allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyImage(s_Data->Allocator, image, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	void* VulkanAllocator::MapMemory(VmaAllocation allocation)
	{
		void* data;
		vmaMapMemory(s_Data->Allocator, allocation, &data);
		return data;
	}

	void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_Data->Allocator, allocation);
	}
}
