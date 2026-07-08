#include "pch.h"
#include "Renderer/RenderPass.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
	RenderPass::RenderPass(std::string name)
		: m_Name(std::move(name))
	{
		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();

		const uint32_t maxFrames = dm->GetParams().MaxFramesInFlight;
		m_TimerQueries.resize(maxFrames);
		m_LastTimes.resize(maxFrames);

		for (uint32_t i = 0; i < maxFrames; i++)
			m_TimerQueries[i] = device->createTimerQuery();
	}

	auto RenderPass::Begin(const nvrhi::CommandListHandle& commandList, const uint32_t frameIndex, const std::string& marker) -> void
	{
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		m_Statistics = {};
		commandList->beginTimerQuery(m_TimerQueries.at(frameIndex));
		commandList->beginMarker(marker.empty() ? m_Name.c_str() : marker.c_str());
	}

	auto RenderPass::End(const nvrhi::CommandListHandle& commandList, const uint32_t frameIndex) -> void
	{
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		commandList->endMarker();
		commandList->endTimerQuery(m_TimerQueries.at(frameIndex));
	}

	auto RenderPass::Readback(const uint32_t frameIndex) -> void
	{
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();

		m_LastTimes[frameIndex] = device->getTimerQueryTime(m_TimerQueries.at(frameIndex));
		device->resetTimerQuery(m_TimerQueries.at(frameIndex));
	}
}
