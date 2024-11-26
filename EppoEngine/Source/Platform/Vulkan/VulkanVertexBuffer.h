#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/VertexBuffer.h"

namespace Eppo
{
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		explicit VulkanVertexBuffer(Buffer buffer);
		~VulkanVertexBuffer() override;

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
