#include "pch.h"
#include "Renderer/StorageBuffer.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
    StorageBuffer::StorageBuffer(const uint32_t structStride, const uint64_t initialSize, std::string debugName)
        : m_Stride(structStride), m_DebugName(std::move(debugName))
    {
        EP_PROFILE_FN("StorageBuffer::StorageBuffer");

        m_Size = initialSize >= m_Stride ? initialSize : m_Stride;
        CreateBuffer();
    }

    auto StorageBuffer::SetData(const void* data, const uint64_t size, const uint64_t offset) -> void
    {
        EP_PROFILE_FN("StorageBuffer::SetData");

        if (size == 0 || !data)
            return;

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = device->createCommandList();
        cmd->open();
        SetData(cmd, data, size, offset);
        cmd->close();

        device->executeCommandList(cmd);
    }

    auto StorageBuffer::SetData(const nvrhi::CommandListHandle& cmdList, const void* data, const uint64_t size, const uint64_t offset)
        -> void
    {
        EP_PROFILE_FN("StorageBuffer::SetData");

        if (size == 0 || !data)
            return;

        if (size > m_Size)
        {
            Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
            m_Size = size;
            CreateBuffer();
        }

        cmdList->writeBuffer(m_Buffer, data, size, offset);
    }

    auto StorageBuffer::CreateBuffer() -> void
    {
        EP_PROFILE_FN("StorageBuffer::CreateBuffer");

        const auto device = DeviceManager::Get()->GetDevice();

        nvrhi::BufferDesc bufferDesc{
            .byteSize = m_Size,
            .structStride = m_Stride,
            .debugName = m_DebugName,
            .initialState = nvrhi::ResourceStates::ShaderResource,
            .keepInitialState = true,
        };

        m_Buffer = device->createBuffer(bufferDesc);
    }
}
