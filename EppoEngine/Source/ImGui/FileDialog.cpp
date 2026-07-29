#include "pch.h"
#include "FileDialog.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

namespace Eppo
{
    namespace
    {
        struct PendingDialog
        {
            FileDialog::ResultCallback OnSelect;
            bool Folder = false;
        };

        std::unordered_map<std::string, PendingDialog> s_Dialogs;

        // ImGuiFileDialog silently ignores an initial path that doesn't exist. Resolve to the
        // nearest existing ancestor so the dialog opens where the caller intended.
        auto ExistingDirectory(std::filesystem::path path) -> std::filesystem::path
        {
            while (!path.empty() && !FS::IsDirectory(path))
                path = path.parent_path();
            return path;
        }

        auto QueueDialog(const std::string& key, const std::string& title, const char* filters,
                         const std::filesystem::path& initialDir, FileDialog::ResultCallback onSelect,
                         const ImGuiFileDialogFlags flags, const bool folder) -> void
        {
            const auto directory = ExistingDirectory(initialDir);

            IGFD::FileDialogConfig config;
            config.path = directory.string();
            config.countSelectionMax = 1;
            config.flags = flags;

            s_Dialogs[key] = { std::move(onSelect), folder };
            ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
        }
    }

    auto FileDialog::OpenFile(const std::string& key, const std::string& title, const std::string& filters,
                              const std::filesystem::path& initialDir, ResultCallback onSelect) -> void
    {
        QueueDialog(key, title, filters.c_str(), initialDir, std::move(onSelect), ImGuiFileDialogFlags_Modal, false);
    }

    auto FileDialog::SaveFile(const std::string& key, const std::string& title, const std::string& filters,
                              const std::filesystem::path& initialDir, ResultCallback onSelect) -> void
    {
        QueueDialog(
            key, title, filters.c_str(), initialDir, std::move(onSelect),
            ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite, false
        );
    }

    auto FileDialog::OpenFolder(const std::string& key, const std::string& title,
                                const std::filesystem::path& initialDir, ResultCallback onSelect) -> void
    {
        QueueDialog(key, title, nullptr, initialDir, std::move(onSelect), ImGuiFileDialogFlags_Modal, true);
    }

    auto FileDialog::Render() -> void
    {
        auto* instance = ImGuiFileDialog::Instance();

        // Gather ready callbacks before invoking any, so a callback that itself queues a
        // dialog cannot rehash s_Dialogs and invalidate the iterator mid-loop.
        std::vector<std::pair<ResultCallback, std::filesystem::path>> ready;

        for (auto it = s_Dialogs.begin(); it != s_Dialogs.end();)
        {
            constexpr ImVec2 minSize(700.0f, 400.0f);
            if (!instance->Display(it->first, ImGuiWindowFlags_NoCollapse, minSize))
            {
                ++it;
                continue;
            }

            if (instance->IsOk() && it->second.OnSelect)
            {
                std::filesystem::path result = it->second.Folder ? instance->GetCurrentPath() : instance->GetFilePathName();
                ready.emplace_back(std::move(it->second.OnSelect), std::move(result));
            }

            instance->Close();
            it = s_Dialogs.erase(it);
        }

        for (auto& [callback, path] : ready)
            callback(path);
    }

    auto FileDialog::BuildFilter(const std::string_view label, const std::vector<std::string>& extensions) -> std::string
    {
        if (extensions.empty())
            return {};

        if (extensions.size() == 1)
            return "." + extensions.front();

        std::string filter(label);
        filter += '{';
        for (size_t i = 0; i < extensions.size(); ++i)
        {
            if (i > 0)
                filter += ',';
            filter += '.';
            filter += extensions[i];
        }
        filter += '}';
        return filter;
    }
}
