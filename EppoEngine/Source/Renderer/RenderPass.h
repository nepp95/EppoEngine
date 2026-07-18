#pragma once

#include "Core/Base.h"
#include "Renderer/Pipeline.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
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

	struct RenderPassSpecification
	{
		std::string Name;
		Ref<Pipeline> Pipeline = nullptr;

		bool ClearColorOnLoad = false;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		bool ClearDepthOnLoad = false;
		float DepthClearValue = 1.0f;
		uint32_t StencilClearValue = 0;
	};

	class RenderPass
	{
	public:
		RenderPass() = default;
		explicit RenderPass(RenderPassSpecification spec);
		~RenderPass() = default;

		RenderPass(const RenderPass&) = delete;
		auto operator=(const RenderPass&) -> RenderPass& = delete;
		RenderPass(RenderPass&&) noexcept = default;
		auto operator=(RenderPass&&) noexcept -> RenderPass& = default;

		auto Resize(uint32_t width, uint32_t height) const -> void;

		[[nodiscard]] auto GetSpecification() const -> const RenderPassSpecification& { return m_Specification; }
		[[nodiscard]] auto GetPipeline() const -> const Ref<Pipeline>& { return m_Specification.Pipeline; }
		[[nodiscard]] auto GetName() const -> const std::string& { return m_Specification.Name; }

		auto SetInput(uint32_t set, uint32_t binding, nvrhi::IResource* resource) -> void;
		auto DeclarePushConstants(uint32_t set, uint32_t size) -> void;

		auto Bake() -> void;

		[[nodiscard]] auto GetBindingSets() const -> const nvrhi::BindingSetVector& { return m_BindingSets; }

	private:
		[[nodiscard]] auto IsValid() const -> bool;

	private:
		RenderPassSpecification m_Specification;

		struct BindingInput
		{
			uint32_t Binding = 0;
			nvrhi::IResource* Resource = nullptr;
		};

		std::unordered_map<uint32_t, std::vector<BindingInput>> m_Inputs;
		std::unordered_map<uint32_t, uint32_t> m_PushConstantSizes;

		std::vector<nvrhi::BindingSetHandle> m_OwnedBindingSets;
		nvrhi::BindingSetVector m_BindingSets;
	};
}
