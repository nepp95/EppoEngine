#pragma once

#include "Core/Buffer/StreamWriter.h"

namespace Eppo
{
    class BufferWriter : public StreamWriter
    {
    public:
        explicit BufferWriter(uint64_t size = 1024);
        explicit BufferWriter(Buffer buffer, uint64_t offset = 0);
        ~BufferWriter() override;

        auto WriteData(const char* data, size_t size) -> bool override;

        [[nodiscard]] auto IsStreamGood() const -> bool override { return m_Buffer.Data != nullptr; }
        [[nodiscard]] auto GetStreamPosition() const -> uint64_t override { return m_Offset; }
        [[nodiscard]] auto GetBuffer() const { return Buffer(m_Buffer, m_Offset); }

    private:
        Buffer m_Buffer;
        uint64_t m_Offset = 0;
        bool m_OwningBuffer = false;
    };
}
