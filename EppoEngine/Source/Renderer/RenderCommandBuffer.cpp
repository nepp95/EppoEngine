#include "pch.h"
#include "Renderer/RenderCommandBuffer.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
    RenderCommandBuffer::RenderCommandBuffer()
    {
        EnsureFrameCapacity(DeviceManager::Get()->GetMaxFramesInFlight());
    }

    auto RenderCommandBuffer::Begin(const std::string_view name) -> void
    {
        const auto& dm = DeviceManager::Get();
        EnsureFrameCapacity(dm->GetMaxFramesInFlight());

        const uint32_t frameIndex = dm->GetCurrentFrameIndex();
        EP_ASSERT(frameIndex < m_CommandLists.size());
        EP_ASSERT(m_ActiveFrameIndex == UINT32_MAX);
        m_ActiveFrameIndex = frameIndex;

        if (m_FrameSubmitted.at(frameIndex))
        {
            const auto device = dm->GetDevice();
            m_Timestamps.at(frameIndex) = device->getTimerQueryTime(m_TimerQueries.at(frameIndex));
            device->resetTimerQuery(m_TimerQueries.at(frameIndex));

            for (const auto& timerName : m_SubmittedNamedTimerQueries.at(frameIndex))
            {
                const auto& timerQuery = m_NamedTimerQueries.at(frameIndex).at(timerName);
                m_NamedTimestamps.at(frameIndex)[timerName] = device->getTimerQueryTime(timerQuery);
                device->resetTimerQuery(timerQuery);
            }

            m_SubmittedNamedTimerQueries.at(frameIndex).clear();
            m_FrameSubmitted.at(frameIndex) = false;
        }

        m_ActiveCommandList = m_CommandLists.at(frameIndex);
        EP_ASSERT(m_ActiveCommandList);
        m_ActiveTimerQuery = m_TimerQueries.at(frameIndex);
        m_GraphicsState = {};

        m_ActiveCommandList->open();
        m_ActiveCommandList->beginTimerQuery(m_ActiveTimerQuery);

        m_HasActiveMarker = !name.empty();
        if (m_HasActiveMarker)
            BeginMarker(name);
    }

    auto RenderCommandBuffer::End() -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        if (m_HasActiveMarker)
            EndMarker();
        m_ActiveCommandList->endTimerQuery(m_ActiveTimerQuery);
        m_HasActiveMarker = false;
    }

    auto RenderCommandBuffer::BeginMarker(const std::string_view name) const -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        m_ActiveCommandList->beginMarker(std::string(name).c_str());
    }

    auto RenderCommandBuffer::EndMarker() const -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        m_ActiveCommandList->endMarker();
    }

    auto RenderCommandBuffer::Submit() -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
        const uint32_t frameIndex = m_ActiveFrameIndex;
        EP_ASSERT(frameIndex < m_TimerQueries.size());

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        m_ActiveCommandList->close();
        device->executeCommandList(m_ActiveCommandList);
        m_FrameSubmitted.at(frameIndex) = true;

        m_ActiveCommandList = nullptr;
        m_ActiveTimerQuery = nullptr;
        m_ActiveFrameIndex = UINT32_MAX;
        m_GraphicsState = {};
    }

    auto RenderCommandBuffer::BeginTimerQuery(const std::string& name) -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
        const uint32_t frameIndex = m_ActiveFrameIndex;
        EP_ASSERT(frameIndex < m_NamedTimerQueries.size());

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        auto& timerQuery = m_NamedTimerQueries.at(frameIndex)[name];
        if (!timerQuery)
            timerQuery = device->createTimerQuery();

        m_SubmittedNamedTimerQueries.at(frameIndex).insert(name);
        m_ActiveCommandList->beginTimerQuery(timerQuery);
    }

    auto RenderCommandBuffer::EndTimerQuery(const std::string& name) const -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        EP_ASSERT(m_ActiveFrameIndex != UINT32_MAX);
        const uint32_t frameIndex = m_ActiveFrameIndex;
        EP_ASSERT(frameIndex < m_NamedTimerQueries.size());

        const auto it = m_NamedTimerQueries.at(frameIndex).find(name);
        EP_ASSERT(it != m_NamedTimerQueries.at(frameIndex).end());
        m_ActiveCommandList->endTimerQuery(it->second);
    }

    auto RenderCommandBuffer::SetGraphicsState(const nvrhi::GraphicsState& state) -> void
    {
        m_GraphicsState = state;
    }

    auto RenderCommandBuffer::CommitGraphicsState() const -> void
    {
        EP_ASSERT(m_ActiveCommandList);
        m_ActiveCommandList->setGraphicsState(m_GraphicsState);
    }

    auto RenderCommandBuffer::GetTime(const std::string_view name, const uint32_t frameIndex) const -> float
    {
        if (frameIndex >= m_NamedTimestamps.size())
            return 0.0f;

        const auto& timestamps = m_NamedTimestamps.at(frameIndex);
        if (const auto it = timestamps.find(std::string(name)); it != timestamps.end())
            return it->second;
        return 0.0f;
    }

    auto RenderCommandBuffer::EnsureFrameCapacity(const uint32_t frameCount) -> void
    {
        if (frameCount <= m_CommandLists.size())
            return;

        const auto device = DeviceManager::Get()->GetDevice();
        const size_t previousCount = m_CommandLists.size();
        m_CommandLists.resize(frameCount);
        m_TimerQueries.resize(frameCount);
        m_Timestamps.resize(frameCount);
        m_NamedTimerQueries.resize(frameCount);
        m_NamedTimestamps.resize(frameCount);
        m_FrameSubmitted.resize(frameCount, false);
        m_SubmittedNamedTimerQueries.resize(frameCount);

        for (size_t i = previousCount; i < frameCount; i++)
        {
            m_CommandLists.at(i) = device->createCommandList();
            m_TimerQueries.at(i) = device->createTimerQuery();
        }
    }
}
