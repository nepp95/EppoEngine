#pragma once

#include "Core/Buffer/StreamReader.h"

#include <filesystem>
#include <fstream>

namespace Eppo
{
    class FileStreamReader : public StreamReader
    {
    public:
        explicit FileStreamReader(const std::filesystem::path& path);
        ~FileStreamReader() override = default;

        auto ReadData(char* outData, size_t size) -> bool override;

        [[nodiscard]] auto IsStreamGood() const -> bool override { return m_Stream.good(); }
        [[nodiscard]] auto GetStreamPosition() const -> uint64_t override;

    private:
        std::filesystem::path m_Path;
        std::ifstream m_Stream;
    };
}
