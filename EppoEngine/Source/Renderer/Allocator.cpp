#include "pch.h"
#include "Allocator.h"

#include "Renderer/RendererContext.h"

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

	void Allocator::Init()
	{
		Ref<RendererContext> context = RendererContext::Get();

		s_Data = new AllocatorData();

		VmaAllocatorCreateInfo createInfo{};
		createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		createInfo.instance = RendererContext::GetVulkanInstance();
		createInfo.physicalDevice = context->GetPhysicalDevice()->GetNativeDevice();
		createInfo.device = context->GetLogicalDevice()->GetNativeDevice();

		VK_CHECK(vmaCreateAllocator(&createInfo, &s_Data->Allocator), "Failed to create vma allocator!");

		context->SubmitResourceFree([]()
		{
			EPPO_WARN("Allocator::Shutdown");
			Allocator::Shutdown();
		});
	}

	void Allocator::Shutdown()
	{
		if (s_Data->MemoryUsed() > 0)
			EPPO_WARN("Still {} memory in use by VMA", s_Data->MemoryUsed());

		vmaDestroyAllocator(s_Data->Allocator);
		delete s_Data;
	}

	VmaAllocation Allocator::AllocateBuffer(VkBuffer& buffer, const VkBufferCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateBuffer(s_Data->Allocator, &createInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	void Allocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
	{
		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyBuffer(s_Data->Allocator, buffer, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	VmaAllocation Allocator::AllocateImage(VkImage& image, const VkImageCreateInfo& createInfo, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo  allocationCreateInfo{};
		allocationCreateInfo.usage = usage;

		VmaAllocationInfo allocationInfo{};

		VmaAllocation allocation;
		vmaCreateImage(s_Data->Allocator, &createInfo, &allocationCreateInfo, &image, &allocation, &allocationInfo);

		s_Data->MemoryAllocated += allocationInfo.size;

		return allocation;
	}

	void Allocator::DestroyImage(VkImage image, VmaAllocation allocation)
	{
		VmaAllocationInfo  allocationInfo{};
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocationInfo);

		vmaDestroyImage(s_Data->Allocator, image, allocation);

		s_Data->MemoryFreed += allocationInfo.size;
	}

	void* Allocator::MapMemory(VmaAllocation allocation)
	{
		void* data;
		vmaMapMemory(s_Data->Allocator, allocation, &data);
		return data;
	}

	void Allocator::UnmapMemory(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_Data->Allocator, allocation);
	}
}
