#include "pch.h"
#include "Core/UUID.h"

namespace Eppo
{
    // Reserved UUIDs (1 - 99)
    // 1 - 9: Mesh Primitives

    UUID::UUID()
    {
        while (m_UUID < 100)
            m_UUID = Utils::GenerateRandomUInt64();
    }

    UUID::UUID(uint64_t id)
        : m_UUID(id)
    {}
}