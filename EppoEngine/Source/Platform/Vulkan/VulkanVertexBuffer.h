#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/VertexBuffer.h"

namespace Eppo
{
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		explicit VulkanVertexBuffer(uint32_t size);
		explicit VulkanVertexBuffer(Buffer buffer);
		~VulkanVertexBuffer() override;

		void SetData(Buffer buffer) override;
		VkBuffer GetBuffer() const { return m_Buffer; }

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
