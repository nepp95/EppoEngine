#include "StatusBar.h"

#include <imgui.h>

#include <EppoEngine.h>

#include <format>
#include <string>
#include <unordered_map>

namespace Eppo
{
    namespace
    {
        constexpr float StatusBarHeight = 24.0f;
        constexpr float PopupWidth = 360.0f;
        constexpr float PopupMaxHeight = 240.0f;
    }

    auto StatusBar::Render() -> void
    {
        // GetPendingTasksCount() is the authoritative "is the pool busy" signal: it
        // counts every queued/in-flight task via atomics, including unnamed tasks
        // that do not appear in any snapshot, and returns to 0 when the pool is idle.
        // Snapshots drive only the per-group detail in the drop-up.
        auto& threadPool = *Application::Get().GetThreadPool();
        const uint32_t inFlight = threadPool.GetPendingTasksCount();
        std::unordered_map<std::string, TaskGroupSnapshot> snapshots = threadPool.GetTaskGroupSnapshots();

        // Drop finished groups from the popup list so it doesn't grow unbounded.
        std::erase_if(
            snapshots,
            [](const auto& kv)
            {
                return kv.second.IsFinished();
            }
        );

        const bool hasTasks = inFlight > 0;

        // The strip occupies the bottom of the DockSpace host window (the host is
        // the current window here; EditorLayer reserves the space by sizing the
        // DockSpace smaller and calling Render() before the host's End()).
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();
        const ImVec2 barMin = { winPos.x, winPos.y + winSize.y - StatusBarHeight };
        const ImVec2 barMax = { winPos.x + winSize.x, winPos.y + winSize.y };

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        drawList->AddLine(barMin, { barMax.x, barMin.y }, ImGui::GetColorU32(ImGuiCol_Separator));

        // Running indicator: an accent dot to the left of the label when busy.
        if (hasTasks)
        {
            const ImVec2 dot = { barMin.x + 12.0f, barMin.y + StatusBarHeight * 0.5f };
            drawList->AddCircleFilled(dot, 4.0f, ImGui::GetColorU32(ImVec4(0.91f, 0.39f, 0.11f, 1.0f)));
        }

        // Clickable flat button spanning the bar. Only opens the drop-up when there
        // are tasks to show; an idle bar stays non-interactive.
        const std::string label = hasTasks ? std::format("{} background task{} running##statusbar", inFlight, inFlight == 1 ? "" : "s")
                                           : std::string("No background tasks running##statusbar");

        ImGui::SetCursorScreenPos(barMin);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));
        ImGui::Button(label.c_str(), ImVec2(winSize.x, StatusBarHeight));
        ImGui::PopStyleColor(3);

        // Open the drop-up on click. The anchor (top-left of the popup, growing
        // upward) is recomputed and re-applied every frame the popup is open so it
        // stays locked to the bar instead of drifting.
        const ImVec2 popupAnchor = { barMin.x, barMin.y - 1.0f };

        if (hasTasks && ImGui::IsItemClicked() && !m_PopupOpen)
        {
            ImGui::OpenPopup("##TaskListPopup");
            m_PopupOpen = true;
        }

        DrawTaskListPopup(popupAnchor, std::move(snapshots), inFlight);
    }

    auto
    StatusBar::DrawTaskListPopup(const ImVec2& anchorPos, std::unordered_map<std::string, TaskGroupSnapshot> snapshots, uint32_t inFlight)
        -> void
    {
        if (!m_PopupOpen)
            return;

        // Re-pin the popup above the bar every frame: pivot (0,1) places the
        // window's bottom-left at anchorPos so it grows upward and stays locked.
        ImGui::SetNextWindowPos(anchorPos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(PopupWidth, 0.0f), ImVec2(PopupWidth, PopupMaxHeight));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::BeginPopup("##TaskListPopup", flags))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Background Tasks");
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("(%u in flight)", inFlight);
            ImGui::Separator();

            if (snapshots.empty())
            {
                ImGui::TextDisabled("No background tasks");
                if (inFlight > 0)
                    ImGui::TextDisabled("%u unnamed task(s) running", inFlight);
            }
            else
            {
                for (const auto& [name, snapshot] : snapshots)
                {
                    const auto pending = snapshot.Pending.load(std::memory_order_relaxed);
                    const auto running = snapshot.Running.load(std::memory_order_relaxed);
                    const auto completed = snapshot.Completed.load(std::memory_order_relaxed);
                    const auto failed = snapshot.Failed.load(std::memory_order_relaxed);
                    const auto cancelled = snapshot.Cancelled.load(std::memory_order_relaxed);
                    const auto total = snapshot.Total.load(std::memory_order_relaxed);

                    ImGui::TextUnformatted(name.c_str());
                    // Progress = completed / total. Pending and Running are shown as
                    // a status line so a partially-dispatched group is distinguishable
                    // from a stalled one.
                    const float frac = total > 0 ? static_cast<float>(completed) / static_cast<float>(total) : 0.0f;
                    ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::TextDisabled("%u/%u", completed, total);

                    std::string status;
                    if (running > 0)
                        status += std::format("{} running", running);
                    if (pending > 0)
                    {
                        if (!status.empty())
                            status += ", ";
                        status += std::format("{} pending", pending);
                    }
                    if (failed > 0)
                    {
                        if (!status.empty())
                            status += ", ";
                        status += std::format("{} failed", failed);
                    }
                    if (cancelled > 0)
                    {
                        if (!status.empty())
                            status += ", ";
                        status += std::format("{} cancelled", cancelled);
                    }
                    if (completed > 0)
                    {
                        if (!status.empty())
                            status += ", ";
                        status += std::format("{} completed", completed);
                    }
                    if (!status.empty())
                    {
                        ImGui::Indent();
                        ImGui::TextDisabled("%s", status.c_str());
                        ImGui::Unindent();
                    }
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            // BeginPopup returned false: closed by outside click or Esc.
            m_PopupOpen = false;
        }
    }
}
