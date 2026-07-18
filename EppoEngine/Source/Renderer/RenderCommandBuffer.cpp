#include "pch.h"
#include "Renderer/RenderCommandBuffer.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
	RenderCommandBuffer::RenderCommandBuffer()
	{
		EnsureBackBufferCapacity(DeviceManager::Get()->GetBackBufferCount());
	}

	auto RenderCommandBuffer::Begin(const std::string_view name) -> void
	{
		const auto& dm = DeviceManager::Get();
		EnsureBackBufferCapacity(dm->GetBackBufferCount());
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_CommandLists.size());

		m_ActiveCommandList = m_CommandLists.at(frameIndex);
		EP_ASSERT(m_ActiveCommandList);
		m_ActiveTimerQuery = m_TimerQueries.at(frameIndex);
		m_GraphicsState = {};

		m_ActiveCommandList->open();
		m_ActiveCommandList->beginTimerQuery(m_ActiveTimerQuery);

		m_HasActiveMarker = !name.empty();
		if (m_HasActiveMarker)
			m_ActiveCommandList->beginMarker(std::string(name).c_str());
	}

	auto RenderCommandBuffer::End() -> void
	{
		EP_ASSERT(m_ActiveCommandList);
		if (m_HasActiveMarker)
			m_ActiveCommandList->endMarker();
		m_ActiveCommandList->endTimerQuery(m_ActiveTimerQuery);
		m_HasActiveMarker = false;
	}

	auto RenderCommandBuffer::Submit() -> void
	{
		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		EP_ASSERT(m_ActiveCommandList);
		m_ActiveCommandList->close();
		device->executeCommandList(m_ActiveCommandList);

		m_Timestamps.at(frameIndex) = device->getTimerQueryTime(m_TimerQueries.at(frameIndex));
		device->resetTimerQuery(m_TimerQueries.at(frameIndex));

		for (const auto& [name, timerQuery] : m_NamedTimerQueries.at(frameIndex))
		{
			m_NamedTimestamps.at(frameIndex)[name] = device->getTimerQueryTime(timerQuery);
			device->resetTimerQuery(timerQuery);
		}

		m_ActiveCommandList = nullptr;
		m_ActiveTimerQuery = nullptr;
	}

	auto RenderCommandBuffer::BeginTimerQuery(const std::string& name) -> void
	{
		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_NamedTimerQueries.size());

		auto& timerQuery = m_NamedTimerQueries.at(frameIndex)[name];
		if (!timerQuery)
			timerQuery = device->createTimerQuery();

		m_ActiveCommandList->beginTimerQuery(timerQuery);
	}

	auto RenderCommandBuffer::EndTimerQuery(const std::string& name) const -> void
	{
		const auto& dm = DeviceManager::Get();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
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

	auto RenderCommandBuffer::EnsureBackBufferCapacity(const uint32_t backBufferCount) -> void
	{
		if (backBufferCount <= m_CommandLists.size())
			return;

		const auto device = DeviceManager::Get()->GetDevice();
		const size_t previousCount = m_CommandLists.size();
		m_CommandLists.resize(backBufferCount);
		m_TimerQueries.resize(backBufferCount);
		m_Timestamps.resize(backBufferCount);
		m_NamedTimerQueries.resize(backBufferCount);
		m_NamedTimestamps.resize(backBufferCount);

		for (size_t i = previousCount; i < backBufferCount; i++)
		{
			m_CommandLists.at(i) = device->createCommandList();
			m_TimerQueries.at(i) = device->createTimerQuery();
		}
	}
}
