#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/IndexBuffer.h"

namespace Eppo
{
	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		explicit VulkanIndexBuffer(uint32_t size);
		explicit VulkanIndexBuffer(Buffer buffer);
		~VulkanIndexBuffer() override;

		void SetData(Buffer buffer) override;

		VkBuffer GetBuffer() const { return m_Buffer; }
		uint32_t GetIndexCount() const override { return m_Size / sizeof(uint32_t); }

	private:
		void CopyWithStagingBuffer(Buffer buffer) const;

	private:
		uint32_t m_Size;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;

		bool m_IsMemoryMapped;
		void* m_MappedMemory = nullptr;
	};
}
