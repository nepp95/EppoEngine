#include "pch.h"
#include "Core/Buffer/BufferWriter.h"

namespace Eppo
{
    BufferWriter::BufferWriter(const uint64_t size)
        : m_OwningBuffer(true)
    {
        m_Buffer = Buffer(size);
    }

    BufferWriter::BufferWriter(const Buffer buffer, const uint64_t offset)
        : m_Buffer(buffer), m_Offset(offset)
    {}

    BufferWriter::~BufferWriter()
    {
        if (m_OwningBuffer)
            m_Buffer.Release();
    }

    auto BufferWriter::WriteData(const char* data, size_t size) -> bool
    {
        const bool valid = size <= m_Buffer.Size && m_Offset <= m_Buffer.Size - size;
        EP_ASSERT(valid);
        if (!valid)
            return false;

        if (size == 0)
            return true;

        std::memcpy(m_Buffer.As<uint8_t>() + m_Offset, data, size);
        m_Offset += size;

        return true;
    }
}
