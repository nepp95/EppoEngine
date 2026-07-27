#pragma once

#include "Core/Buffer/StreamWriter.h"

#include <filesystem>
#include <fstream>

namespace Eppo
{
    class FileStreamWriter : public StreamWriter
    {
    public:
        explicit FileStreamWriter(const std::filesystem::path& path);
        ~FileStreamWriter() override = default;

        auto WriteData(const char* data, size_t size) -> bool override;

        [[nodiscard]] auto IsStreamGood() const -> bool override { return m_Stream.good(); }
        [[nodiscard]] auto GetStreamPosition() const -> uint64_t override;

    private:
        std::filesystem::path m_Path;
        std::ofstream m_Stream;
    };
}
