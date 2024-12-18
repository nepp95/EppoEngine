#pragma once

#include "Platform/Vulkan/DescriptorAllocator.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer();
		~VulkanRenderer() override = default;

		void Shutdown() override;

		// Render queue commands
		void ExecuteRenderCommands() override;
		void SubmitCommand(RenderCommand command) override;

		// Render passes
		void BeginRenderPass(const Ref<CommandBuffer>& commandBuffer, const Ref<Pipeline>& pipeline) override;
		void EndRenderPass(const Ref<CommandBuffer>& commandBuffer) override;

		// Shaders
		Ref<Shader> GetShader(const std::string& name) override { return m_ShaderLibrary.Get(name); }
		void* AllocateDescriptor(void* layout) override;

	private:
		CommandQueue m_CommandQueue;
		ShaderLibrary m_ShaderLibrary;

		std::array<DescriptorAllocator, VulkanConfig::MaxFramesInFlight> m_DescriptorAllocators;

		RenderStatistics m_RenderStatistics;

		static bool s_IsInstantiated;
	};
}
