#pragma once

#include <nfd.hpp>

namespace Eppo
{
    class FileDialog
    {
    public:
        static auto OpenFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir = {})
            -> std::filesystem::path;
        static auto SaveFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir = {})
            -> std::filesystem::path;
        static auto OpenFolder(const std::filesystem::path& initialDir = {}) -> std::filesystem::path;
    };
}
