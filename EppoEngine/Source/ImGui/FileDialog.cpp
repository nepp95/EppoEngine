#include "pch.h"
#include "FileDialog.h"

namespace Eppo
{
    namespace Utils
    {
        // NFD silently ignores a default path that doesn't exist and reopens the OS
        // "recently used" folder instead, which is what made dialogs appear to open in
        // random places. Resolve to the nearest existing directory so it has something
        // valid to honour.
        static auto ExistingDirectory(std::filesystem::path path) -> std::filesystem::path
        {
            while (!path.empty() && !FS::IsDirectory(path))
                path = path.parent_path();
            return path;
        }
    }

    auto FileDialog::OpenFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir) -> std::filesystem::path
    {
        const auto directory = Utils::ExistingDirectory(initialDir);

        NFD::UniquePath nfdPath = nullptr;
        auto result = NFD::OpenDialog(
            nfdPath, filters.data(), static_cast<uint32_t>(filters.size()), directory.empty() ? nullptr : directory.string().c_str()
        );

        std::filesystem::path outPath = {};
        if (result == NFD_OKAY)
            outPath = nfdPath.get();
        else if (result == NFD_ERROR)
            Log::Error("NFD Failed: {}", NFD::GetError());

        return outPath;
    }

    auto FileDialog::SaveFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir) -> std::filesystem::path
    {
        const auto directory = Utils::ExistingDirectory(initialDir);

        NFD::UniquePath nfdPath = nullptr;
        auto result = NFD::SaveDialog(
            nfdPath, filters.data(), static_cast<uint32_t>(filters.size()), directory.empty() ? nullptr : directory.string().c_str()
        );

        std::filesystem::path outPath = {};
        if (result == NFD_OKAY)
            outPath = nfdPath.get();
        else if (result == NFD_ERROR)
            Log::Error("NFD Failed: {}", NFD::GetError());

        return outPath;
    }

    auto FileDialog::OpenFolder(const std::filesystem::path& initialDir) -> std::filesystem::path
    {
        const auto directory = Utils::ExistingDirectory(initialDir);

        NFD::UniquePath nfdPath = nullptr;
        const auto result = NFD::PickFolder(nfdPath, directory.empty() ? nullptr : directory.string().c_str());

        std::filesystem::path outPath;
        if (result == NFD_OKAY)
            outPath = nfdPath.get();
        else if (result == NFD_ERROR)
            Log::Error("NFD Failed: {}", NFD::GetError());
        return outPath;
    }
}
