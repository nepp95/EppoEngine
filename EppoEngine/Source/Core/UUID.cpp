#include "pch.h"
#include "Core/UUID.h"

namespace Eppo
{
    // Reserved UUIDs (1 - 99)
    // 1 = Cone Mesh
    // 2 = Cube Mesh
    // 3 = Cylinder Mesh
    // 4 = Sphere Mesh
    // 5 = Capsule Mesh
    // 6-9 reserved for mesh primitives
    // 10 = Placeholder Texture

    UUID::UUID()
    {
        while (m_UUID < 100)
            m_UUID = Utils::GenerateRandomUInt64();
    }

    UUID::UUID(uint64_t id)
        : m_UUID(id)
    {}
}
