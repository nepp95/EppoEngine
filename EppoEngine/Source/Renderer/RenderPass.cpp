#include "pch.h"
#include "Renderer/RenderPass.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Framebuffer.h"

#include <nvrhi/utils.h>

namespace Eppo
{
	RenderPass::RenderPass(RenderPassSpecification spec)
		: m_Specification(std::move(spec))
	{
		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();

		const uint32_t maxFrames = dm->GetParams().MaxFramesInFlight;
		m_TimerQueries.resize(maxFrames);
		m_Timestamps.resize(maxFrames);

		for (uint32_t i = 0; i < maxFrames; i++)
			m_TimerQueries[i] = device->createTimerQuery();
	}

	auto RenderPass::Begin(const nvrhi::CommandListHandle& commandList, const std::string& marker) -> nvrhi::GraphicsState
	{
		const auto& dm = DeviceManager::Get();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		m_Statistics = {};
		commandList->open();
		commandList->beginTimerQuery(m_TimerQueries.at(frameIndex));
		commandList->beginMarker(marker.empty() ? m_Specification.Name.c_str() : marker.c_str());

		nvrhi::GraphicsState state{};

		if (m_Specification.Pipeline)
		{
			const auto& pipeline = m_Specification.Pipeline;
			const auto& framebuffer = pipeline->GetSpecification().Framebuffer;

			if (m_Specification.ClearColor)
			{
				const auto& clearColor = framebuffer->GetSpecification().ClearColor;
				for (uint32_t i = 0; i < framebuffer->GetFramebuffer()->getDesc().colorAttachments.size(); i++)
					nvrhi::utils::ClearColorAttachment(commandList, framebuffer->GetFramebuffer(), i, nvrhi::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
			}

			if (m_Specification.ClearDepth)
			{
				const auto& spec = framebuffer->GetSpecification();
				nvrhi::utils::ClearDepthStencilAttachment(commandList, framebuffer->GetFramebuffer(), spec.DepthClearValue, spec.StencilClearValue);
			}

			const auto width = static_cast<float>(pipeline->GetWidth());
			const auto height = static_cast<float>(pipeline->GetHeight());

			state.pipeline = pipeline->GetPipeline();
			state.framebuffer = framebuffer->GetFramebuffer();
			state.viewport.viewports = { nvrhi::Viewport(width, height) };
			state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(width), static_cast<int>(height)) };
		}

		return state;
	}

	auto RenderPass::End(const nvrhi::CommandListHandle& commandList) -> void
	{
		const auto& dm = DeviceManager::Get();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		commandList->endMarker();
		commandList->endTimerQuery(m_TimerQueries.at(frameIndex));
	}

	auto RenderPass::Submit(const nvrhi::CommandListHandle& commandList) -> void
	{
		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < m_TimerQueries.size());

		commandList->close();
		device->executeCommandList(commandList);

		m_Timestamps[frameIndex] = device->getTimerQueryTime(m_TimerQueries.at(frameIndex));
		device->resetTimerQuery(m_TimerQueries.at(frameIndex));
	}

	auto RenderPass::Resize(const uint32_t width, const uint32_t height) const -> void
	{
		if (m_Specification.Pipeline)
			m_Specification.Pipeline->Resize(width, height);
	}
}
