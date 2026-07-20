#include "Panels/LogPanel.h"

namespace Eppo
{
    namespace
    {
        constexpr size_t MAX_ENTRIES = LOG_BUFFER_CAPACITY + LOG_BUFFER_CAPACITY / 2;

        auto LevelBit(const spdlog::level::level_enum level) -> uint8_t
        {
            const auto bucket = level == spdlog::level::critical ? spdlog::level::err : level;
            return static_cast<uint8_t>(1u << static_cast<uint32_t>(bucket));
        }

        auto SourceBit(const LogSource source) -> uint8_t
        {
            return static_cast<uint8_t>(1u << static_cast<uint32_t>(source));
        }

        auto LevelColor(const spdlog::level::level_enum level) -> ImVec4
        {
            switch (level)
            {
                case spdlog::level::trace: return { 0.55f, 0.55f, 0.55f, 1.0f };
                case spdlog::level::warn: return { 1.0f, 0.80f, 0.25f, 1.0f };
                case spdlog::level::err:
                case spdlog::level::critical: return { 1.0f, 0.35f, 0.35f, 1.0f };
                default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            }
        }

        auto MaskToggle(const char* label, uint8_t& mask, const uint8_t bit) -> bool
        {
            bool enabled = (mask & bit) != 0;
            if (!ImGui::Checkbox(label, &enabled))
                return false;

            mask = enabled ? static_cast<uint8_t>(mask | bit) : static_cast<uint8_t>(mask & ~bit);
            return true;
        }
    }

    auto LogPanel::RenderGui() -> void
    {
        ScopedBegin scopedBegin("Log");

        RenderToolbar();
        ImGui::Separator();

        SyncEntries();
        RenderEntries();
    }

    auto LogPanel::RenderToolbar() -> void
    {
        m_FilterDirty |= MaskToggle("Trace", m_LevelMask, LevelBit(spdlog::level::trace));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Info", m_LevelMask, LevelBit(spdlog::level::info));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Warn", m_LevelMask, LevelBit(spdlog::level::warn));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Error", m_LevelMask, LevelBit(spdlog::level::err));

        m_FilterDirty |= MaskToggle("Core", m_SourceMask, SourceBit(LogSource::Core));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Glfw", m_SourceMask, SourceBit(LogSource::Glfw));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Script", m_SourceMask, SourceBit(LogSource::Script));
        ImGui::SameLine();
        m_FilterDirty |= MaskToggle("Vulkan", m_SourceMask, SourceBit(LogSource::Vulkan));

        if (m_TextFilter.Draw("##log_filter", -1.0f))
            m_FilterDirty = true;
    }

    auto LogPanel::SyncEntries() -> void
    {
        const auto& buffer = Log::GetBuffer();
        if (!buffer)
            return;

        // Only the new messages are copied under the sink lock; filtering and drawing run unlocked.
        const size_t firstNew = m_Entries.size();
        m_Version = buffer->CopySince(m_Version, m_Entries);

        if (m_Entries.size() > MAX_ENTRIES)
        {
            const auto excess = static_cast<std::ptrdiff_t>(m_Entries.size() - LOG_BUFFER_CAPACITY);
            m_Entries.erase(m_Entries.begin(), m_Entries.begin() + excess);
            m_FilterDirty = true;
        }

        if (m_FilterDirty)
        {
            m_FilteredIndices.clear();
            AppendFiltered(0);
            m_FilterDirty = false;
        }
        else
        {
            AppendFiltered(firstNew);
        }
    }

    auto LogPanel::AppendFiltered(const size_t firstIndex) -> void
    {
        for (size_t i = firstIndex; i < m_Entries.size(); ++i)
        {
            const LogEntry& entry = m_Entries[i];

            if ((m_LevelMask & LevelBit(entry.Level)) == 0)
                continue;
            if ((m_SourceMask & SourceBit(entry.Source)) == 0)
                continue;
            if (!m_TextFilter.PassFilter(entry.Text.data(), entry.Text.data() + entry.Text.size()))
                continue;

            m_FilteredIndices.push_back(static_cast<uint32_t>(i));
        }
    }

    auto LogPanel::RenderEntries() -> void
    {
        ImGui::BeginChild("##log_entries", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_FilteredIndices.size()));

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const LogEntry& entry = m_Entries[m_FilteredIndices[row]];

                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.Level));
                ImGui::TextUnformatted(entry.Text.data(), entry.Text.data() + entry.Text.size());
                ImGui::PopStyleColor();
            }
        }

        // ScrollMaxY is still last frame's, so this asks whether we were at the bottom before this frame's rows.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
}
