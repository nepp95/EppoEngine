#include "pch.h"
#include "Renderer/Renderer.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"

#include <nvrhi/utils.h>
#include <nvrhi/nvrhi.h>

namespace Eppo
{
	Renderer::Renderer()
	{
		m_DescriptorManager = CreateRef<DescriptorManager>();
	}

	auto Renderer::Init() -> void
	{
		m_ShaderLibrary.Load("geometry");
		m_ShaderLibrary.Load("skybox");
		m_ShaderLibrary.Load("imgui");
		m_ShaderLibrary.Load("wireframe");
	}

	auto Renderer::BeginRenderPass(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<RenderPass>& renderPass) -> void
	{
		const auto& cmd = commandBuffer->GetCommandList();
		const auto& pipeline = renderPass->GetPipeline();
		const auto& framebuffer = pipeline->GetSpecification().Framebuffer;
		const auto& renderPassSpec = renderPass->GetSpecification();

		cmd->beginMarker(renderPassSpec.Name.c_str());

		if (renderPassSpec.ClearColorOnLoad)
		{
			const auto& clearColor = renderPassSpec.ClearColor;
			for (size_t i = 0; i < framebuffer->GetFramebuffer()->getDesc().colorAttachments.size(); i++)
				nvrhi::utils::ClearColorAttachment(cmd, framebuffer->GetFramebuffer(), i, nvrhi::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
		}

		if (renderPassSpec.ClearDepthOnLoad)
			nvrhi::utils::ClearDepthStencilAttachment(cmd, framebuffer->GetFramebuffer(), renderPassSpec.DepthClearValue, renderPassSpec.StencilClearValue);

		auto& graphicsState = commandBuffer->GetGraphicsState();
		graphicsState.pipeline = pipeline->GetPipeline();
		EP_ASSERT(graphicsState.pipeline != nullptr);
		graphicsState.framebuffer = framebuffer->GetFramebuffer();
		EP_ASSERT(graphicsState.framebuffer != nullptr);

		graphicsState.viewport.viewports = { nvrhi::Viewport(static_cast<float>(framebuffer->GetWidth()), static_cast<float>(framebuffer->GetHeight())) };
		graphicsState.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(framebuffer->GetWidth()), static_cast<int>(framebuffer->GetHeight())) };

		graphicsState.bindings = renderPass->GetBindingSets();
		EP_ASSERT(graphicsState.bindings.size() == pipeline->GetPipeline()->getDesc().bindingLayouts.size());

		commandBuffer->CommitGraphicsState();
	}

	auto Renderer::EndRenderPass(const Ref<RenderCommandBuffer>& commandBuffer) -> void
	{
		commandBuffer->GetCommandList()->endMarker();
	}

	auto Renderer::GetShader(const std::string& name) const -> Ref<Shader>
	{
		return m_ShaderLibrary.Get(name);
	}

	auto Renderer::GetDescriptorManager() const -> const Ref<DescriptorManager>&
	{
		return m_DescriptorManager;
	}
}
