#include "pch.h"
#include "Core/Buffer/FileStreamWriter.h"

namespace Eppo
{
    FileStreamWriter::FileStreamWriter(const std::filesystem::path& path)
        : m_Path(path), m_Stream(m_Path, std::ios::binary)
    {
        if (!m_Stream)
            Log::Error("Failed to open file stream writer for path: {}", m_Path);
    }

    auto FileStreamWriter::WriteData(const char* data, const size_t size) -> bool
    {
        m_Stream.write(data, size);
        if (!m_Stream)
        {
            Log::Error("Failed to write {} bytes to '{}'", size, m_Path);
            return false;
        }

        m_Position += size;

        return true;
    }
}
