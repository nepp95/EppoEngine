#include "pch.h"
#include "Renderer/Renderer.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Pipeline.h"
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

	auto Renderer::LoadShaders(const std::map<std::string, PackedShaderData>& packedSources) -> void
	{
		if (packedSources.empty())
		{
		    m_ShaderLibrary.Load("composite");
		    m_ShaderLibrary.Load("geometry");
		    m_ShaderLibrary.Load("imgui");
		    m_ShaderLibrary.Load("skybox");
		    m_ShaderLibrary.Load("wireframe");
		} else
		{
		    for (const auto& [name, packedShader] : packedSources)
		        m_ShaderLibrary.Load(name, packedShader.ShaderSources);
		}
	}

	auto Renderer::Init() -> void
	{
		nvrhi::SamplerDesc samplerDesc{};
		samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
		samplerDesc.setAllFilters(true);
		m_CompositeSampler = DeviceManager::Get()->GetDevice()->createSampler(samplerDesc);
		EP_ASSERT(m_CompositeSampler, "Failed to create the swapchain composite sampler.");

		m_CompositeCommandBuffer = CreateRef<RenderCommandBuffer>();
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

	auto Renderer::CompositeToSwapchain(const Ref<Image>& image) -> void
	{
		EP_PROFILE_FN("Renderer::CompositeToSwapchain")

		EP_ASSERT(image != nullptr, "Cannot composite a null image to the swapchain.");

		const auto& dm = DeviceManager::Get();
		const uint32_t backBufferCount = dm->GetBackBufferCount();
		const uint32_t backBufferIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(backBufferIndex < backBufferCount, "The current swapchain back buffer index is invalid.");

		if (m_CompositePasses.size() != backBufferCount)
		{
			m_CompositePasses.resize(backBufferCount);
			m_CompositeFramebuffers.resize(backBufferCount);
		}

		const auto& framebuffer = dm->GetCurrentSwapchainImage().Framebuffer;
		const nvrhi::FramebufferHandle framebufferHandle = framebuffer->GetFramebuffer();
		Ref<RenderPass>& renderPass = m_CompositePasses.at(backBufferIndex);

	    // Lazy load pipeline since this is only used in the runtime
		if (!renderPass || m_CompositeFramebuffers.at(backBufferIndex) != framebufferHandle)
		{
			const PipelineSpecification pipelineSpec{
				.Shader = m_ShaderLibrary.Get("composite"),
				.Framebuffer = framebuffer,
				.Width = framebuffer->GetWidth(),
				.Height = framebuffer->GetHeight(),
				.CullMode = nvrhi::RasterCullMode::None,
			};

			renderPass = CreateRef<RenderPass>(RenderPassSpecification{
				.Name = "Composite",
				.Pipeline = CreateRef<Pipeline>(pipelineSpec),
			});

			m_CompositeFramebuffers.at(backBufferIndex) = framebufferHandle;
		}

		renderPass->SetInput(0, 0, image->GetTexture());
		renderPass->SetInput(0, 0, m_CompositeSampler);
		renderPass->Bake();

		m_CompositeCommandBuffer->Begin("Composite");
		BeginRenderPass(m_CompositeCommandBuffer, renderPass);

		m_CompositeCommandBuffer->GetCommandList()->draw(nvrhi::DrawArguments{
			.vertexCount = 3,
			.instanceCount = 1,
		});

		EndRenderPass(m_CompositeCommandBuffer);
		m_CompositeCommandBuffer->End();
		m_CompositeCommandBuffer->Submit();
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
