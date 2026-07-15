#pragma once

#include "Core/Base.h"
#include "Renderer/Pipeline.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
	// Accumulated per-pass draw statistics. Reset at the start of each pass and
	// added together to form the scene/grand totals shown in the stats UI.
	struct PassStatistics
	{
		uint32_t DrawCalls = 0;
		uint32_t Meshes = 0;
		uint32_t Submeshes = 0;
		uint32_t Instances = 0;
		uint32_t Vertices = 0;
		uint32_t Indices = 0;

		auto operator+=(const PassStatistics& other) -> PassStatistics&
		{
			DrawCalls += other.DrawCalls;
			Meshes += other.Meshes;
			Submeshes += other.Submeshes;
			Instances += other.Instances;
			Vertices += other.Vertices;
			Indices += other.Indices;
			return *this;
		}
	};

	// Construction parameters for a RenderPass. The pipeline is optional: scene
	// passes own one (RenderPass derives viewport/scissor/clear from it), while
	// ImGuiRenderer passes have no single pipeline and build state per-draw.
	struct RenderPassSpecification
	{
		std::string Name;
		Ref<Pipeline> Pipeline = nullptr;
		bool ClearColor = false;
		bool ClearDepth = false;
	};

	// Owns the GPU timer-query lifecycle, draw statistics, debug markers, and —
	// when a pipeline is provided — the base GraphicsState (pipeline, framebuffer,
	// viewport, scissor) and optional framebuffer clear for a single render pass.
	// Drive it with Begin/End/Submit around the pass' command list.
	class RenderPass
	{
	public:
		RenderPass() = default;
		explicit RenderPass(RenderPassSpecification spec);

		// Owns per-frame GPU timer queries; copying would silently share them
		// between two logical passes. Non-copyable; movable so SceneRenderer can
		// assign passes in its constructor body.
		RenderPass(const RenderPass&) = delete;
		auto operator=(const RenderPass&) -> RenderPass& = delete;
		RenderPass(RenderPass&&) noexcept = default;
		auto operator=(RenderPass&&) noexcept -> RenderPass& = default;

		auto Begin(const nvrhi::CommandListHandle& commandList) -> nvrhi::GraphicsState;
		auto End(const nvrhi::CommandListHandle& commandList) -> void;
		auto Submit(const nvrhi::CommandListHandle& commandList) -> void;

		auto Resize(uint32_t width, uint32_t height) const -> void;

		[[nodiscard]] auto GetSpecification() const -> const RenderPassSpecification& { return m_Specification; }
		[[nodiscard]] auto GetPipeline() const -> const Ref<Pipeline>& { return m_Specification.Pipeline; }
		[[nodiscard]] auto GetStats() -> PassStatistics& { return m_Statistics; }
		[[nodiscard]] auto GetStats() const -> const PassStatistics& { return m_Statistics; }
		[[nodiscard]] auto GetTime(const uint32_t frameIndex) const -> float { return m_Timestamps.at(frameIndex); }
		[[nodiscard]] auto GetTimeMs(const uint32_t frameIndex) const -> float { return m_Timestamps.at(frameIndex) * 1000.0f; }
		[[nodiscard]] auto GetName() const -> const std::string& { return m_Specification.Name; }

	private:
		RenderPassSpecification m_Specification;
		std::vector<nvrhi::TimerQueryHandle> m_TimerQueries;
		std::vector<float> m_Timestamps;
		PassStatistics m_Statistics;
	};
}
