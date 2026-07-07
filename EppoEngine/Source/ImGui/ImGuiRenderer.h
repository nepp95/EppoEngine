#pragma once

#include "Renderer/IndexBuffer.h"
#include "Renderer/VertexBuffer.h"

#include <imgui.h>
#include <nvrhi/nvrhi.h>

#include <map>

namespace Eppo
{
	class Swapchain;

	class ImGuiRenderer
	{
	public:
		ImGuiRenderer();

		auto Resize() -> void;
		auto UpdateFontTexture() -> void;
		auto RenderToSwapchain(ImGuiViewport* viewport, const ScopedPtr<Swapchain>& swapchain) -> void;
		auto Render(ImGuiViewport* viewport, nvrhi::GraphicsPipelineHandle pipeline, nvrhi::FramebufferHandle framebuffer) -> void;
		auto GetGPUTime(uint32_t frameIndex) const -> float;
		auto GetOwnGPUTime(uint32_t frameIndex) const -> float;

	private:
		auto UpdateGeometry(ImDrawData* drawData) -> void;
		auto ReallocateBuffer(uint64_t size, bool indexBuffer) -> nvrhi::BufferHandle;
		auto GetOrCreatePipeline(const ScopedPtr<Swapchain>& swapchain) -> nvrhi::GraphicsPipelineHandle;
		auto GetOrCreateBindingSet(nvrhi::TextureHandle texture) -> nvrhi::BindingSetHandle;

	private:
		nvrhi::CommandListHandle m_CommandList = nullptr;
		std::vector<nvrhi::TimerQueryHandle> m_TimerQueries;
		std::vector<float> m_LastQueryTimes;

		nvrhi::GraphicsPipelineDesc m_PipelineDesc;

		struct PipelineCache
		{
			std::array<nvrhi::FramebufferHandle, 3> Framebuffers;
			std::array<nvrhi::GraphicsPipelineHandle, 3> Pipelines;
		};
		std::map<Swapchain*, PipelineCache> m_PipelineCache;

		nvrhi::BindingLayoutHandle m_BindingSetLayout = nullptr;

		nvrhi::BufferHandle m_VertexBuffer = nullptr;
		nvrhi::BufferHandle m_IndexBuffer = nullptr;
		std::vector<ImDrawVert> m_LocalVertexData;
		std::vector<ImDrawIdx> m_LocalIndexData;

		nvrhi::TextureHandle m_FontTexture = nullptr;
		nvrhi::SamplerHandle m_FontSampler = nullptr;

		std::unordered_map<nvrhi::TextureHandle, nvrhi::BindingSetHandle> m_BindingSetCache;
	};
}