#include "pch.h"
#include "Core/Buffer/StreamWriter.h"

namespace Eppo
{
    auto StreamWriter::WriteZero(const uint64_t size) -> bool
    {
        constexpr char zero = 0;
        for (uint64_t i = 0; i < size; i++)
        {
            if (!WriteData(&zero, 1))
                return false;
        }

        return true;
    }

    auto StreamWriter::WriteString(const std::string_view str) -> bool
    {
        const uint64_t size = str.size();
        return WriteRaw(size) && WriteData(str.data(), sizeof(char) * size);
    }

    auto StreamWriter::WriteBuffer(Buffer buffer) -> bool
    {
        return WriteRaw(buffer.Size) && WriteData(buffer.As<const char>(), buffer.Size);
    }
}
