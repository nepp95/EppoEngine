#include "pch.h"
#include "Renderer/VertexBuffer.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"

namespace Eppo
{
    VertexBuffer::VertexBuffer(uint64_t size)
        : m_Size(size), m_CpuWritable(true)
    {
        EP_PROFILE_FN("VertexBuffer::VertexBuffer");

        CreateBuffer();
    }

    VertexBuffer::VertexBuffer(const void* data, uint64_t size)
        : m_Size(size)
    {
        EP_PROFILE_FN("VertexBuffer::VertexBuffer");

        CreateBuffer();
        SetData(data, m_Size);
    }

    auto VertexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) -> void
    {
        EP_PROFILE_FN("VertexBuffer::SetData")

        EP_ASSERT(UINT64_MAX - size > offset);

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmdList = device->createCommandList();

        if (size > m_Size)
        {
            Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
            m_Size = size;
            CreateBuffer();
        }

        cmdList->open();
        cmdList->writeBuffer(m_Buffer, data, size, offset);
        cmdList->close();

        device->executeCommandList(cmdList);
    }

    auto VertexBuffer::CreateBuffer(bool isResize) -> void
    {
        EP_PROFILE_FN("VertexBuffer::CreateBuffer");

        const auto device = DeviceManager::Get()->GetDevice();

        nvrhi::BufferDesc bufferDesc{
            .byteSize = m_Size,
            .debugName = "VertexBuffer",
            .isVertexBuffer = true,
            .initialState = nvrhi::ResourceStates::VertexBuffer,
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

    auto VertexBuffer::GeneratePrimitive(const MeshPrimitiveType type) -> Ref<VertexBuffer>
    {
        EP_PROFILE_FN("VertexBuffer::GeneratePrimitive");

        switch (type)
        {
            case MeshPrimitiveType::Cone:
            {
                constexpr uint32_t sectors = 36;
                constexpr float radius = 1.0f;
                constexpr float height = 2.0f;
                constexpr float halfHeight = height / 2.0f;
                constexpr float pi = 3.14159265358979323846f;

                std::vector<Vertex> vertices;
                vertices.reserve((sectors + 1) * 3 + 1);

                // Side
                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const glm::vec3 normal = glm::normalize(glm::vec3(height * glm::cos(theta), radius, height * glm::sin(theta)));
                    vertices.emplace_back(
                        Vertex{
                            { 0.0f, halfHeight, 0.0f },
                            normal
                    }
                    );
                }

                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    const glm::vec3 normal = glm::normalize(glm::vec3(height * glm::cos(theta), radius, height * glm::sin(theta)));
                    vertices.emplace_back(
                        Vertex{
                            { x, -halfHeight, z },
                            normal
                    }
                    );
                }

                // Base
                vertices.emplace_back(
                    Vertex{
                        { 0.0f, -halfHeight, 0.0f },
                        { 0.0f, -1.0f,       0.0f }
                }
                );
                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    vertices.emplace_back(
                        Vertex{
                            { x,    -halfHeight, z    },
                            { 0.0f, -1.0f,       0.0f }
                    }
                    );
                }

                return CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
            }

            case MeshPrimitiveType::Cube:
            {
                constexpr std::array vertices = {
                    Vertex{ { -1.0f, -1.0f, 1.0f },  { 0.0f, 0.0f, 1.0f }  },
                    Vertex{ { 1.0f, -1.0f, 1.0f },   { 0.0f, 0.0f, 1.0f }  },
                    Vertex{ { 1.0f, 1.0f, 1.0f },    { 0.0f, 0.0f, 1.0f }  },
                    Vertex{ { -1.0f, 1.0f, 1.0f },   { 0.0f, 0.0f, 1.0f }  },
                    Vertex{ { 1.0f, -1.0f, -1.0f },  { 0.0f, 0.0f, -1.0f } },
                    Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
                    Vertex{ { -1.0f, 1.0f, -1.0f },  { 0.0f, 0.0f, -1.0f } },
                    Vertex{ { 1.0f, 1.0f, -1.0f },   { 0.0f, 0.0f, -1.0f } },
                    Vertex{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },
                    Vertex{ { -1.0f, -1.0f, 1.0f },  { -1.0f, 0.0f, 0.0f } },
                    Vertex{ { -1.0f, 1.0f, 1.0f },   { -1.0f, 0.0f, 0.0f } },
                    Vertex{ { -1.0f, 1.0f, -1.0f },  { -1.0f, 0.0f, 0.0f } },
                    Vertex{ { 1.0f, -1.0f, 1.0f },   { 1.0f, 0.0f, 0.0f }  },
                    Vertex{ { 1.0f, -1.0f, -1.0f },  { 1.0f, 0.0f, 0.0f }  },
                    Vertex{ { 1.0f, 1.0f, -1.0f },   { 1.0f, 0.0f, 0.0f }  },
                    Vertex{ { 1.0f, 1.0f, 1.0f },    { 1.0f, 0.0f, 0.0f }  },
                    Vertex{ { -1.0f, 1.0f, 1.0f },   { 0.0f, 1.0f, 0.0f }  },
                    Vertex{ { 1.0f, 1.0f, 1.0f },    { 0.0f, 1.0f, 0.0f }  },
                    Vertex{ { 1.0f, 1.0f, -1.0f },   { 0.0f, 1.0f, 0.0f }  },
                    Vertex{ { -1.0f, 1.0f, -1.0f },  { 0.0f, 1.0f, 0.0f }  },
                    Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f } },
                    Vertex{ { 1.0f, -1.0f, -1.0f },  { 0.0f, -1.0f, 0.0f } },
                    Vertex{ { 1.0f, -1.0f, 1.0f },   { 0.0f, -1.0f, 0.0f } },
                    Vertex{ { -1.0f, -1.0f, 1.0f },  { 0.0f, -1.0f, 0.0f } },
                };

                return CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
            }

            case MeshPrimitiveType::Cylinder:
            {
                constexpr uint32_t sectors = 36;
                constexpr float radius = 1.0f;
                constexpr float height = 2.0f;
                constexpr float halfHeight = height / 2.0f;
                constexpr float pi = 3.14159265358979323846f;

                std::vector<Vertex> vertices;
                vertices.reserve((sectors + 1) * 4 + 2);

                // Side wall
                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    const glm::vec3 normal = glm::vec3(x, 0.0f, z) / radius;
                    vertices.emplace_back(
                        Vertex{
                            { x, -halfHeight, z },
                            normal
                    }
                    );
                }

                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    const glm::vec3 normal = glm::vec3(x, 0.0f, z) / radius;
                    vertices.emplace_back(
                        Vertex{
                            { x, halfHeight, z },
                            normal
                    }
                    );
                }

                // Bottom cap
                vertices.emplace_back(
                    Vertex{
                        { 0.0f, -halfHeight, 0.0f },
                        { 0.0f, -1.0f,       0.0f }
                }
                );
                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    vertices.emplace_back(
                        Vertex{
                            { x,    -halfHeight, z    },
                            { 0.0f, -1.0f,       0.0f }
                    }
                    );
                }

                // Top cap
                vertices.emplace_back(
                    Vertex{
                        { 0.0f, halfHeight, 0.0f },
                        { 0.0f, 1.0f,       0.0f }
                }
                );
                for (uint32_t j = 0; j <= sectors; j++)
                {
                    const float theta = static_cast<float>(j) * (2.0f * pi / sectors);
                    const float x = radius * glm::cos(theta);
                    const float z = radius * glm::sin(theta);
                    vertices.emplace_back(
                        Vertex{
                            { x,    halfHeight, z    },
                            { 0.0f, 1.0f,       0.0f }
                    }
                    );
                }

                return CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
            }

            case MeshPrimitiveType::Sphere:
            {
                constexpr uint32_t sectors = 36; // Longitude
                constexpr uint32_t stacks = 18; // Latitude
                constexpr float radius = 1.0f;
                constexpr float pi = 3.14159265358979323846f;

                std::vector<Vertex> vertices;
                vertices.reserve((stacks + 1) * (sectors + 1));

                for (uint32_t i = 0; i <= stacks; i++)
                {
                    const float stackAngle = pi / 2.0f - static_cast<float>(i) * (pi / stacks);
                    const float xy = radius * glm::cos(stackAngle);
                    const float z = radius * glm::sin(stackAngle);

                    for (uint32_t j = 0; j <= sectors; j++)
                    {
                        const float sectorAngle = static_cast<float>(j) * (2.0f * pi / sectors);
                        const float x = xy * glm::cos(sectorAngle);
                        const float y = xy * glm::sin(sectorAngle);
                        const glm::vec3 position = { x, y, z };
                        const glm::vec3 normal = position / radius;
                        vertices.emplace_back(Vertex{ position, normal });
                    }
                }

                return CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
            }

            case MeshPrimitiveType::Capsule:
            {
                constexpr uint32_t sectors = 36; // Longitude
                constexpr uint32_t hemiStacks = 8; // Latitude rings per hemisphere
                constexpr float radius = 1.0f;
                constexpr float halfHeight = 1.0f; // Half the cylinder segment; hemisphere centers sit at +-halfHeight.
                constexpr float pi = 3.14159265358979323846f;

                std::vector<Vertex> vertices;
                vertices.reserve(2 * (hemiStacks + 1) * (sectors + 1));

                // Surface of revolution built north->south so winding matches Sphere: a
                // hemisphere cap, the shared cylinder wall (the two equator rings), then
                // the other cap. The equator rings (lat 0) carry the cylinder's normals.
                const auto addRing = [&vertices](const float centerY, const float lat)
                {
                    const float ringY = centerY + radius * glm::sin(lat);
                    const float ringR = radius * glm::cos(lat);
                    for (uint32_t j = 0; j <= sectors; j++)
                    {
                        const float sector = static_cast<float>(j) * (2.0f * pi / sectors);
                        const float cx = glm::cos(sector);
                        const float sz = glm::sin(sector);
                        const glm::vec3 position = { ringR * cx, ringY, ringR * sz };
                        const glm::vec3 normal = glm::normalize(glm::vec3(glm::cos(lat) * cx, glm::sin(lat), glm::cos(lat) * sz));
                        vertices.emplace_back(Vertex{ position, normal });
                    }
                };

                for (uint32_t i = 0; i <= hemiStacks; i++) // Top hemisphere: lat +PI/2 -> 0
                    addRing(halfHeight, pi / 2.0f - static_cast<float>(i) * (pi / 2.0f / hemiStacks));

                for (uint32_t i = 0; i <= hemiStacks; i++) // Bottom hemisphere: lat 0 -> -PI/2
                    addRing(-halfHeight, -static_cast<float>(i) * (pi / 2.0f / hemiStacks));

                return CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
            }
        }
    }
}