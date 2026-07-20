#pragma once

#include "Panels/Panel.h"

#include <imgui.h>

namespace Eppo
{
    class LogPanel : public Panel
    {
    public:
        LogPanel() = default;
        virtual ~LogPanel() = default;

        auto RenderGui() -> void override;

    private:
        auto RenderToolbar() -> void;
        auto SyncEntries() -> void;
        auto AppendFiltered(size_t firstIndex) -> void;
        auto RenderEntries() -> void;

        uint8_t m_LevelMask = 0xFF;
        uint8_t m_SourceMask = 0xFF;
        ImGuiTextFilter m_TextFilter;

        std::vector<LogEntry> m_Entries;
        std::vector<uint32_t> m_FilteredIndices;
        uint64_t m_Version = 0;
        bool m_FilterDirty = true;
    };
}
