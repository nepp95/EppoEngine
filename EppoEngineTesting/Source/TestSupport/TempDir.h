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
        explicit TempDir(const std::filesystem::path& parent = std::filesystem::temp_directory_path())
        {
            m_Path = parent / ("eppo-test-" + std::to_string(Utils::GenerateRandomUInt64()));
            std::filesystem::create_directories(m_Path);
        }

        ~TempDir()
        {
            // Never throw out of the destructor: a directory a test left watched or
            // otherwise open can fail to remove, and that must not abort the run.
            std::error_code error;
            std::filesystem::remove_all(m_Path, error);
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        [[nodiscard]] auto Path() const -> const std::filesystem::path& { return m_Path; }
        [[nodiscard]] auto File(const std::string& name) const -> std::filesystem::path { return m_Path / name; }

    private:
        std::filesystem::path m_Path;
    };
}
