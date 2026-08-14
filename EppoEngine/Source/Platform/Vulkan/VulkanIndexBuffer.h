#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Renderer/IndexBuffer.h"

namespace Eppo
{
    class VulkanIndexBuffer final : public IndexBuffer
    {
    public:
        explicit VulkanIndexBuffer(uint32_t size);
        explicit VulkanIndexBuffer(Buffer buffer);
        VulkanIndexBuffer(const VulkanIndexBuffer&) = delete;
        VulkanIndexBuffer(const VulkanIndexBuffer&&) = delete;
        VulkanIndexBuffer& operator=(const VulkanIndexBuffer&) = delete;
        VulkanIndexBuffer& operator=(const VulkanIndexBuffer&&) = delete;
        ~VulkanIndexBuffer() override;

        void SetData(Buffer buffer) override;

        [[nodiscard]] VkBuffer GetBuffer() const { return m_Buffer; }
        [[nodiscard]] uint32_t GetIndexCount() const override { return m_Size / sizeof(uint32_t); }

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
