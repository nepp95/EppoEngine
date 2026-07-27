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

        return true;
    }

    auto FileStreamReader::GetStreamPosition() const -> uint64_t
    {
        const auto position = const_cast<std::ifstream&>(m_Stream).tellg();
        return position < 0 ? 0 : static_cast<uint64_t>(position);
    }
}
