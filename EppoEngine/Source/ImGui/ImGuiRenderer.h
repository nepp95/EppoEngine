#pragma once

#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"

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
		auto RenderToSwapchain(ImGuiViewport* viewport, const ScopedPtr<Swapchain>& swapchain, bool clearSwapchainTarget = true) -> void;
		auto Render(ImGuiViewport* viewport, const Ref<Pipeline>& pipeline, bool clearTarget = true) -> void;

	    [[nodiscard]] auto GetGPUTime(uint32_t frameIndex) const -> float;
		[[nodiscard]] auto GetOwnGPUTime(uint32_t frameIndex) const -> float;
		[[nodiscard]] auto GetStats() const -> PassStatistics;
		[[nodiscard]] auto GetOwnStats() const -> const PassStatistics& { return m_Stats; }

	private:
		auto UpdateGeometry(ImDrawData* drawData) -> void;
		auto ReallocateBuffer(uint64_t size, bool indexBuffer) -> nvrhi::BufferHandle;
		auto GetOrCreatePipeline(const ScopedPtr<Swapchain>& swapchain) -> const Ref<Pipeline>&;
		auto GetOrCreateBindingSet(const nvrhi::TextureHandle& texture) -> nvrhi::BindingSetHandle;

	private:
		nvrhi::CommandListHandle m_CommandList = nullptr;

		RenderCommandBuffer m_RenderCommandBuffer;
		PassStatistics m_Stats{};

		// Template spec (no framebuffer) cloned per swapchain in GetOrCreatePipeline.
		PipelineSpecification m_PipelineSpecTemplate{};

		struct PipelineCache
		{
			std::array<nvrhi::FramebufferHandle, 3> Framebuffers;
			std::array<Ref<Pipeline>, 3> Pipelines;
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
