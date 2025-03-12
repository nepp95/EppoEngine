#pragma once

#include <filesystem>

namespace Eppo
{
    class FileDialog
    {
    public:
        // TODO: Implement ourselves
        static std::filesystem::path OpenFile(const char* filter, const std::filesystem::path& initialDir)
        {
            return {};
        }

        static std::filesystem::path SaveFile(const char* filter)
        {
            return {};
        }
    };
}
