#pragma once

#include <nvrhi/nvrhi.h>

#include <string_view>

namespace Eppo
{
    class RenderCommandBuffer
    {
    public:
        RenderCommandBuffer();

        RenderCommandBuffer(const RenderCommandBuffer&) = delete;
        auto operator=(const RenderCommandBuffer&) -> RenderCommandBuffer& = delete;
        RenderCommandBuffer(RenderCommandBuffer&&) noexcept = default;
        auto operator=(RenderCommandBuffer&&) noexcept -> RenderCommandBuffer& = default;

        auto Begin(std::string_view name = {}) -> void;
        auto End() -> void;
        auto Submit() -> void;

        auto BeginTimerQuery(const std::string& name) -> void;
        auto EndTimerQuery(const std::string& name) const -> void;

        [[nodiscard]] auto GetGraphicsState() -> nvrhi::GraphicsState& { return m_GraphicsState; }
        auto SetGraphicsState(const nvrhi::GraphicsState& state) -> void;
        auto CommitGraphicsState() const -> void;

        [[nodiscard]] auto GetCommandList() -> const nvrhi::CommandListHandle& { return m_ActiveCommandList; }

        [[nodiscard]] auto GetTime(const uint32_t frameIndex) const -> float
        {
            return frameIndex < m_Timestamps.size() ? m_Timestamps[frameIndex] : 0.0f;
        }
        [[nodiscard]] auto GetTimeMs(const uint32_t frameIndex) const -> float { return GetTime(frameIndex) * 1000.0f; }
        [[nodiscard]] auto GetTime(std::string_view name, uint32_t frameIndex) const -> float;
        [[nodiscard]] auto GetTimeMs(const std::string_view name, const uint32_t frameIndex) const -> float
        {
            return GetTime(name, frameIndex) * 1000.0f;
        }

    private:
        auto EnsureBackBufferCapacity(uint32_t backBufferCount) -> void;

    private:
        std::vector<nvrhi::CommandListHandle> m_CommandLists;
        nvrhi::CommandListHandle m_ActiveCommandList = nullptr;

        std::vector<nvrhi::TimerQueryHandle> m_TimerQueries;
        nvrhi::TimerQueryHandle m_ActiveTimerQuery = nullptr;
        std::vector<float> m_Timestamps;

        std::vector<std::unordered_map<std::string, nvrhi::TimerQueryHandle>> m_NamedTimerQueries;
        std::vector<std::unordered_map<std::string, float>> m_NamedTimestamps;

        nvrhi::GraphicsState m_GraphicsState{};
        bool m_HasActiveMarker = false;
    };
}
