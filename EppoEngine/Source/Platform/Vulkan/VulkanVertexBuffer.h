#pragma once

#include "Core/Buffer.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/VertexBuffer.h"

namespace Eppo
{
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(void* data, uint32_t size);
		virtual ~VulkanVertexBuffer();

		VkBuffer GetBuffer() const { return m_Buffer; }
		void RT_Bind(Ref<CommandBuffer> commandBuffer) const override;

	private:
		void CreateBuffer(VmaMemoryUsage usage);

	private:
		Buffer m_LocalStorage;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
