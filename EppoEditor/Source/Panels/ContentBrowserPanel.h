#pragma once

#include "Panels/Panel.h"

#include <functional>

namespace Eppo
{
    class Image;

    // Project asset browser modelled after the Unreal Content Browser / Unity
    // Project window: a folder tree on the left and a thumbnail grid on the
    // right, rooted at the active project's Assets directory. Registered assets
    // can be dragged out to other panels (e.g. a mesh onto a MeshComponent).
    class ContentBrowserPanel : public Panel
    {
    public:
        using OpenSceneFn = std::function<void(AssetHandle)>;

        ContentBrowserPanel() = default;
        ~ContentBrowserPanel() = default;

        auto RenderGui() -> void override;

        // Wired by the editor so double-clicking a scene routes through the
        // authoritative EditorLayer::OpenScene (which also rebuilds scripting).
        auto SetOpenSceneCallback(OpenSceneFn fn) -> void { m_OpenScene = std::move(fn); }

    private:
        auto SyncToActiveProject() -> void;
        auto LoadIcons() -> void;
        auto GetIcon(AssetType type) const -> const Ref<Image>&;

        auto DrawTopBar() -> void;
        auto DrawFolderTree(const std::filesystem::path& directory) -> void;
        auto DrawContents() -> void;
        auto DrawContextMenus() -> void;

        auto OpenAsset(const std::filesystem::path& path) -> void;
        auto ImportFile() -> void;
        auto DeletePath(const std::filesystem::path& path) -> void;

    private:
        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;

        bool m_IconsLoaded = false;
        Ref<Image> m_DirectoryIcon;
        Ref<Image> m_FileIcon;
        Ref<Image> m_MeshIcon;
        Ref<Image> m_SceneIcon;
        Ref<Image> m_ScriptIcon;
        Ref<Image> m_TextureIcon;
        Ref<Image> m_UnknownIcon;

        char m_SearchBuffer[128] = {};

        // Deferred actions: context-menu clicks stash a target here and the popup
        // is driven after the item loop to keep ImGui's id stack happy.
        std::filesystem::path m_RenameTarget;
        std::string m_RenameBuffer;
        bool m_OpenRenamePopup = false;
        std::filesystem::path m_DeleteTarget;
        bool m_OpenDeletePopup = false;

        OpenSceneFn m_OpenScene;
    };
}
