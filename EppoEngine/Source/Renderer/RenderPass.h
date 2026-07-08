#pragma once

#include <nvrhi/nvrhi.h>

#include <string>
#include <vector>

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

	// Bundles the GPU timer-query lifecycle (one query per frame-in-flight) and
	// the draw statistics for a single render pass, centralizing the boilerplate
	// that SceneRenderer and ImGuiRenderer previously each duplicated. Construct
	// one per logical pass; drive it from the pass' command list.
	class RenderPass
	{
	public:
		explicit RenderPass(std::string name);

		// Owns per-frame GPU timer queries; copying would silently share them
		// between two logical passes. Non-copyable (and non-movable — instances
		// live as members, never reseated).
		RenderPass(const RenderPass&) = delete;
		auto operator=(const RenderPass&) -> RenderPass& = delete;

		// Reset the stats, begin the GPU timer and open a debug marker on the
		// (already-open) command list. Pass a non-empty marker to override the pass
		// name in the capture (used for ImGui's per-viewport markers).
		auto Begin(const nvrhi::CommandListHandle& commandList, uint32_t frameIndex, const std::string& marker = "") -> void;
		// Close the debug marker and end the GPU timer.
		auto End(const nvrhi::CommandListHandle& commandList, uint32_t frameIndex) -> void;
		// Read the timer back and reset the query. Call after the command list that
		// wrapped Begin/End has been executed.
		auto Readback(uint32_t frameIndex) -> void;

		[[nodiscard]] auto Stats() -> PassStatistics& { return m_Statistics; }
		[[nodiscard]] auto GetStats() const -> const PassStatistics& { return m_Statistics; }
		[[nodiscard]] auto GetTimeMs(const uint32_t frameIndex) const -> float { return m_LastTimesMs.at(frameIndex); }
		[[nodiscard]] auto GetName() const -> const std::string& { return m_Name; }

	private:
		std::string m_Name;
		std::vector<nvrhi::TimerQueryHandle> m_TimerQueries;
		std::vector<float> m_LastTimesMs;
		PassStatistics m_Statistics;
	};
}
