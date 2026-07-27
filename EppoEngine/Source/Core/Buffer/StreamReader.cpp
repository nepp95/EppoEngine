#include "pch.h"
#include "Core/Buffer/StreamReader.h"

namespace Eppo
{
    auto StreamReader::ReadString(std::string& str) -> bool
    {
        uint64_t size = 0;
        if (!ReadRaw(size))
            return false;

        str.resize(size);
        return ReadData(str.data(), sizeof(char) * size);
    }

    auto StreamReader::ReadBuffer(Buffer& buffer) -> bool
    {
        uint64_t size = 0;
        if (!ReadRaw(size))
            return false;

        buffer.Allocate(size);
        return ReadData(buffer.As<char>(), size);
    }
}
