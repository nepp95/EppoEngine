#pragma once

#include <EppoEngine.h>

#include <unordered_map>

namespace Eppo
{
    class StatusBar
    {
    public:
        auto Render() -> void;

    private:
        auto DrawTaskListPopup(const ImVec2& anchorPos, std::unordered_map<std::string, TaskGroupSnapshot> snapshots, uint32_t inFlight)
            -> void;

        bool m_PopupOpen = false;
    };
}
