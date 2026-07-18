#pragma once

#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	class DescriptorManager;

	class Renderer
	{
	public:
		Renderer();

		// DeviceManager publishes Renderer before this loads shaders that need its descriptor manager.
		auto Init() -> void;

		static auto BeginRenderPass(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<RenderPass>& renderPass) -> void;
		static auto EndRenderPass(const Ref<RenderCommandBuffer>& commandBuffer) -> void;

		[[nodiscard]] auto GetShader(const std::string& name) const -> Ref<Shader>;
		[[nodiscard]] auto GetDescriptorManager() const -> const Ref<DescriptorManager>&;

	private:
		ShaderLibrary m_ShaderLibrary;
		Ref<DescriptorManager> m_DescriptorManager = nullptr;
	};
}
