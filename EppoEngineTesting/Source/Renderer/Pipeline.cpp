#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Renderer.h"

using namespace Eppo;

SUITE(Renderer)
{
	TEST(Pipeline_GeometryBindingLayoutsAreInSetOrder)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& renderer = DeviceManager::Get()->GetRenderer();

		const FramebufferSpecification framebufferSpec{
			.Width = 256,
			.Height = 256,
			.Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
			.DebugName = "Framebuffer PipelineTest",
		};

		const PipelineSpecification pipelineSpec{
			.Shader = renderer->GetShader("geometry"),
			.Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
			.Width = 256,
			.Height = 256,
			.CullMode = nvrhi::RasterCullMode::Front,
			.DepthTestEnable = true,
			.DepthWriteEnable = true,
		};

		const auto pipeline = CreateRef<Pipeline>(pipelineSpec);
		const auto& shaderLayouts = pipeline->GetSpecification().Shader->GetBindingLayouts();
		const auto& pipelineLayouts = pipeline->GetPipeline()->getDesc().bindingLayouts;

		CHECK_EQUAL(3u, static_cast<uint32_t>(pipelineLayouts.size()));
		CHECK(pipelineLayouts[0].Get() == shaderLayouts.at(0).Get());
		CHECK(pipelineLayouts[1].Get() == shaderLayouts.at(1).Get());
		CHECK(pipelineLayouts[2].Get() == shaderLayouts.at(2).Get());
	}

	TEST(Pipeline_CustomBlendStateCreatesValidHandle)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& renderer = DeviceManager::Get()->GetRenderer();

		const FramebufferSpecification framebufferSpec{
			.Width = 256,
			.Height = 256,
			.Attachments = { nvrhi::Format::RGBA8_UNORM },
			.DebugName = "Framebuffer BlendTest",
		};

		nvrhi::BlendState blendState;
		blendState.targets[0].blendEnable = true;
		blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
		blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;

		const PipelineSpecification pipelineSpec{
			.Shader = renderer->GetShader("imgui"),
			.Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
			.Width = 256,
			.Height = 256,
			.CullMode = nvrhi::RasterCullMode::None,
			.BlendState = blendState,
		};

		const auto pipeline = CreateRef<Pipeline>(pipelineSpec);
		CHECK(pipeline->GetPipeline());
	}
}
