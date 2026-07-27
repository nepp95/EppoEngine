#include "pch.h"
#include "Core/Buffer/FileStreamReader.h"

namespace Eppo
{
    FileStreamReader::FileStreamReader(const std::filesystem::path& path)
        : m_Path(path), m_Stream(m_Path, std::ios::binary)
    {
        if (!m_Stream)
            Log::Error("Failed to open file stream reader for path: {}", m_Path);
    }

    auto FileStreamReader::ReadData(char* outData, const size_t size) -> bool
    {
        m_Stream.read(outData, size);
        if (!m_Stream)
        {
            Log::Error("Failed to read {} bytes from '{}'", size, m_Path);
            return false;
        }

        m_Position += size;

        return true;
    }
}
