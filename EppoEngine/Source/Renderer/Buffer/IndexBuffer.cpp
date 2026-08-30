#include "pch.h"
#include "Renderer/Buffer/IndexBuffer.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Mesh.h"

namespace Eppo
{
    IndexBuffer::IndexBuffer(uint64_t size)
        : m_Size(size), m_CpuWritable(true)
    {
        EP_PROFILE_FN("IndexBuffer::IndexBuffer");

        CreateBuffer();
    }

    IndexBuffer::IndexBuffer(const void* data, uint64_t size)
        : m_Size(size)
    {
        EP_PROFILE_FN("IndexBuffer::IndexBuffer");

        CreateBuffer();
        SetData(data, m_Size);
    }

    auto IndexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) -> void
    {
        EP_PROFILE_FN("IndexBuffer::SetData")

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = device->createCommandList();

        if (size > m_Size)
        {
            Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
            m_Size = size;
            CreateBuffer();
        }

        cmd->open();
        cmd->writeBuffer(m_Buffer, data, m_Size, offset);
        cmd->close();

        device->executeCommandList(cmd);
    }

    auto IndexBuffer::CreateBuffer(bool isResize) -> void
    {
        EP_PROFILE_FN("IndexBuffer::CreateBuffer");

        const auto device = DeviceManager::Get()->GetDevice();

        nvrhi::BufferDesc bufferDesc{
            .byteSize = m_Size,
            .debugName = "IndexBuffer",
            .isIndexBuffer = true,
            .initialState = nvrhi::ResourceStates::IndexBuffer,
            .keepInitialState = true,
        };

        if (m_CpuWritable)
            bufferDesc.cpuAccess = nvrhi::CpuAccessMode::Write;

        auto buffer = device->createBuffer(bufferDesc);
        if (isResize)
        {
            const nvrhi::CommandListHandle cmdList = device->createCommandList();
            cmdList->open();
            cmdList->copyBuffer(buffer, 0, m_Buffer, 0, m_Buffer->getDesc().byteSize);
            cmdList->close();
            device->executeCommandList(cmdList);
        }

        m_Buffer = buffer;
    }

    auto IndexBuffer::GeneratePrimitive(const MeshPrimitiveType type) -> Ref<IndexBuffer>
    {
        EP_PROFILE_FN("IndexBuffer::GeneratePrimitive");

        switch (type)
        {
            case MeshPrimitiveType::Cone:
            {
                constexpr uint32_t sectors = 36;
                constexpr uint32_t ringVertexCount = sectors + 1;
                constexpr uint32_t apexStart = 0;
                constexpr uint32_t baseStart = ringVertexCount;
                constexpr uint32_t capCenter = ringVertexCount * 2;
                constexpr uint32_t capRingStart = capCenter + 1;

                std::vector<uint32_t> indices;
                indices.reserve(sectors * 6);

                // Side
                for (uint32_t j = 0; j < sectors; j++)
                {
                    indices.emplace_back(apexStart + j);
                    indices.emplace_back(baseStart + j + 1);
                    indices.emplace_back(baseStart + j);
                }

                // Base
                for (uint32_t j = 0; j < sectors; j++)
                {
                    indices.emplace_back(capCenter);
                    indices.emplace_back(capRingStart + j);
                    indices.emplace_back(capRingStart + j + 1);
                }

                return CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
            }

            case MeshPrimitiveType::Cube:
            {
                constexpr std::array indices = { 0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
                                                 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23 };

                return CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
            }

            case MeshPrimitiveType::Cylinder:
            {
                constexpr uint32_t sectors = 36;
                constexpr uint32_t ringVertexCount = sectors + 1;
                constexpr uint32_t sideBottomStart = 0;
                constexpr uint32_t sideTopStart = ringVertexCount;
                constexpr uint32_t bottomCapCenter = ringVertexCount * 2;
                constexpr uint32_t bottomCapRingStart = bottomCapCenter + 1;
                constexpr uint32_t topCapCenter = bottomCapRingStart + ringVertexCount;
                constexpr uint32_t topCapRingStart = topCapCenter + 1;

                std::vector<uint32_t> indices;
                indices.reserve(sectors * 12);

                // Side wall
                for (uint32_t j = 0; j < sectors; j++)
                {
                    const uint32_t b1 = sideBottomStart + j;
                    const uint32_t b2 = sideBottomStart + j + 1;
                    const uint32_t t1 = sideTopStart + j;
                    const uint32_t t2 = sideTopStart + j + 1;

                    indices.emplace_back(b1);
                    indices.emplace_back(t1);
                    indices.emplace_back(b2);
                    indices.emplace_back(t1);
                    indices.emplace_back(t2);
                    indices.emplace_back(b2);
                }

                // Bottom cap
                for (uint32_t j = 0; j < sectors; j++)
                {
                    indices.emplace_back(bottomCapCenter);
                    indices.emplace_back(bottomCapRingStart + j);
                    indices.emplace_back(bottomCapRingStart + j + 1);
                }

                // Top cap
                for (uint32_t j = 0; j < sectors; j++)
                {
                    indices.emplace_back(topCapCenter);
                    indices.emplace_back(topCapRingStart + j + 1);
                    indices.emplace_back(topCapRingStart + j);
                }

                return CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
            }

            case MeshPrimitiveType::Sphere:
            {
                constexpr uint32_t sectors = 36; // Longitude
                constexpr uint32_t stacks = 18; // Latitude

                std::vector<uint32_t> indices;
                indices.reserve(stacks * sectors * 6);

                for (uint32_t i = 0; i < stacks; i++)
                {
                    uint32_t k1 = i * (sectors + 1);
                    uint32_t k2 = k1 + sectors + 1;

                    for (uint32_t j = 0; j < sectors; j++, k1++, k2++)
                    {
                        if (i != 0)
                        {
                            indices.emplace_back(k1);
                            indices.emplace_back(k2);
                            indices.emplace_back(k1 + 1);
                        }

                        if (i != (stacks - 1))
                        {
                            indices.emplace_back(k1 + 1);
                            indices.emplace_back(k2);
                            indices.emplace_back(k2 + 1);
                        }
                    }
                }

                return CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
            }

            case MeshPrimitiveType::Capsule:
            {
                constexpr uint32_t sectors = 36; // Longitude
                constexpr uint32_t hemiStacks = 8; // Latitude rings per hemisphere
                constexpr uint32_t rings = 2 * (hemiStacks + 1);

                std::vector<uint32_t> indices;
                indices.reserve((rings - 1) * sectors * 6);

                // Same grid stitching as Sphere; the degenerate pole rings (first/last)
                // are skipped so no zero-area triangles are emitted at the caps' tips.
                for (uint32_t i = 0; i < rings - 1; i++)
                {
                    uint32_t k1 = i * (sectors + 1);
                    uint32_t k2 = k1 + sectors + 1;

                    for (uint32_t j = 0; j < sectors; j++, k1++, k2++)
                    {
                        if (i != 0)
                        {
                            indices.emplace_back(k1);
                            indices.emplace_back(k2);
                            indices.emplace_back(k1 + 1);
                        }

                        if (i != rings - 2)
                        {
                            indices.emplace_back(k1 + 1);
                            indices.emplace_back(k2);
                            indices.emplace_back(k2 + 1);
                        }
                    }
                }

                return CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
            }
        }
    }
}
