#pragma once

#include "Core/Buffer/StreamReader.h"

namespace Eppo
{
    class BufferReader : public StreamReader
    {
    public:
        explicit BufferReader(Buffer buffer, uint64_t offset = 0);

        auto ReadData(char* outData, size_t size) -> bool override;

        [[nodiscard]] auto IsStreamGood() const -> bool override { return m_Buffer.Data != nullptr; }
        [[nodiscard]] auto GetStreamPosition() const -> uint64_t override { return m_Offset; }
        [[nodiscard]] auto GetBuffer() const { return Buffer(m_Buffer, m_Offset); }

    private:
        Buffer m_Buffer;
        uint64_t m_Offset = 0;
    };
}
