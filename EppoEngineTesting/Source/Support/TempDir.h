#pragma once

#include "Utility/Random.h"

#include <filesystem>
#include <string>

namespace Eppo::Testing
{
    // RAII scratch directory under the system temp path; removed recursively on
    // destruction, so tests leave nothing behind even if an assertion aborts.
    class TempDir
    {
    public:
        TempDir()
        {
            m_Path = std::filesystem::temp_directory_path()
                / ("eppo-test-" + std::to_string(Utils::GenerateRandomUInt64()));
            std::filesystem::create_directories(m_Path);
        }

        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(m_Path, ec);
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        [[nodiscard]] auto Path() const -> const std::filesystem::path& { return m_Path; }
        [[nodiscard]] auto File(const std::string& name) const -> std::filesystem::path { return m_Path / name; }

    private:
        std::filesystem::path m_Path;
    };
}
