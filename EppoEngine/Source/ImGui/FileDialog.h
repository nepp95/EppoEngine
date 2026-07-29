#pragma once

namespace Eppo
{
    class FileDialog
    {
    public:
        using ResultCallback = std::function<void(const std::filesystem::path&)>;

        // Each call queues an in-editor ImGui dialog keyed by `key`; `onSelect` fires from
        // Render() when the user confirms. Cancel does nothing. `filters` uses ImGuiFileDialog
        // syntax (compose it with BuildFilter); the folder picker takes no filters.
        static auto OpenFile(const std::string& key, const std::string& title, const std::string& filters,
                             const std::filesystem::path& initialDir, ResultCallback onSelect) -> void;
        static auto SaveFile(const std::string& key, const std::string& title, const std::string& filters,
                             const std::filesystem::path& initialDir, ResultCallback onSelect) -> void;
        static auto OpenFolder(const std::string& key, const std::string& title,
                               const std::filesystem::path& initialDir, ResultCallback onSelect) -> void;

        // Display any queued dialog and dispatch its callback. Call once per frame.
        static auto Render() -> void;

        // Compose an ImGuiFileDialog filter from a display label and dot-less extensions:
        // none -> "", one -> ".ext", many -> "Label{.e1,.e2}".
        [[nodiscard]] static auto BuildFilter(std::string_view label, const std::vector<std::string>& extensions) -> std::string;
    };
}
