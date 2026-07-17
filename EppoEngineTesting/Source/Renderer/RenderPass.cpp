#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

using namespace Eppo;

SUITE(Renderer)
{
    namespace
    {
        // Minimal owned-pipeline pass for testing: geometry shader, 256x256
        // RGBA8+D32 framebuffer, front-face culling, depth test+write.
        auto MakeTestPipeline() -> Ref<Pipeline>
        {
            const auto& renderer = DeviceManager::Get()->GetRenderer();

            FramebufferSpecification framebufferSpec{
                .Width = 256,
                .Height = 256,
                .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
                .ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
                .ClearColorOnLoad = true,
                .ClearDepthOnLoad = true,
            };

            PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("geometry"),
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = 256,
                .Height = 256,
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            return CreateRef<Pipeline>(pipelineSpec);
        }
    }

    TEST(RenderPass_OwnedPipeline_BeginReturnsBaseState)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto device = DeviceManager::Get()->GetDevice();
		const auto pipeline = MakeTestPipeline();

		RenderPass pass(RenderPassSpecification{
			.Name = "TestPass",
			.Pipeline = pipeline,
			.ClearColor = true,
			.ClearDepth = true,
		});

		CHECK(pass.GetPipeline().get() == pipeline.get());
		CHECK_EQUAL(std::string("TestPass"), pass.GetName());

		const auto cl = device->createCommandList();
		const nvrhi::GraphicsState state = pass.Begin(cl);

		// Begin must pre-populate the base state from the owned pipeline so call
		// sites don't repeat pipeline/framebuffer/viewport/scissor setup.
		CHECK(state.pipeline == pipeline->GetPipeline());
		CHECK(state.framebuffer == pipeline->GetSpecification().Framebuffer->GetFramebuffer());
		CHECK_EQUAL(256u, static_cast<uint32_t>(state.viewport.viewports[0].width()));
		CHECK_EQUAL(256u, static_cast<uint32_t>(state.viewport.viewports[0].height()));
		CHECK_EQUAL(256, state.viewport.scissorRects[0].width());
		CHECK_EQUAL(256, state.viewport.scissorRects[0].height());

		cl->setGraphicsState(state);
		pass.End(cl);
		pass.Submit(cl);
	}

	TEST(RenderPass_BeginResetsStatsEachCall)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto device = DeviceManager::Get()->GetDevice();
		const auto pipeline = MakeTestPipeline();

		RenderPass pass(RenderPassSpecification{
			.Name = "TestPass",
			.Pipeline = pipeline,
		});

		const auto cl = device->createCommandList();

		// First pass: bump a stat so we can see it reset on the next Begin.
		nvrhi::GraphicsState state = pass.Begin(cl);
		cl->setGraphicsState(state);
		pass.GetStats().DrawCalls = 5;
		pass.End(cl);
		pass.Submit(cl);

		CHECK_EQUAL(5u, pass.GetStats().DrawCalls);

		// Second pass: Begin must zero the stats before the body runs.
		state = pass.Begin(cl);
		cl->setGraphicsState(state);
		CHECK_EQUAL(0u, pass.GetStats().DrawCalls);
		pass.End(cl);
		pass.Submit(cl);
	}

	TEST(RenderPass_Lifecycle_SurvivesMultipleFrames)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto device = DeviceManager::Get()->GetDevice();
		const auto pipeline = MakeTestPipeline();

		RenderPass pass(RenderPassSpecification{
			.Name = "TestPass",
			.Pipeline = pipeline,
		});

		const auto cl = device->createCommandList();

		// Drive a few frames so each frame-in-flight index gets its own
		// Begin/End/Submit cycle. The timer query for a given frame index must
		// read back without throwing (GetTimeMs indexes the per-frame slot).
		for (uint32_t i = 0; i < 3; ++i)
		{
			const uint32_t frameIndex = DeviceManager::Get()->GetCurrentBackBufferIndex();

			nvrhi::GraphicsState state = pass.Begin(cl);
			cl->setGraphicsState(state);
			pass.End(cl);
			pass.Submit(cl);

			// Timer slot for this frame index is valid and non-negative.
			CHECK(pass.GetTimeMs(frameIndex) >= 0.0f);

			Testing::AppHarness::AdvanceFrames(1);
		}
	}

	TEST(RenderPass_NoPipeline_BeginEndSubmitWorks)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto device = DeviceManager::Get()->GetDevice();

		// ImGui-style pass: no owned pipeline, caller builds its own state per
		// draw. Begin/End/Submit must still drive the timer+marker lifecycle.
		RenderPass pass(RenderPassSpecification{ .Name = "UI" });

		CHECK(!pass.GetPipeline());

		const auto cl = device->createCommandList();
		const nvrhi::GraphicsState state = pass.Begin(cl);
		(void)state;
		pass.End(cl);
		pass.Submit(cl);
	}

	TEST(Pipeline_CustomBlendState_CreatesValidHandle)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& renderer = DeviceManager::Get()->GetRenderer();

		FramebufferSpecification framebufferSpec{
			.Width = 64u,
			.Height = 64u,
			.Attachments = { nvrhi::Format::RGBA8_UNORM },
			.DebugName = "Framebuffer BlendTest",
		};

		// Alpha blending, like ImGuiRenderer needs. PipelineSpecification must
		// honor a caller-supplied BlendState instead of hardcoding no-blend.
		nvrhi::BlendState blendState;
		blendState.targets[0].blendEnable = true;
		blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
		blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;

		PipelineSpecification pipelineSpec{
			.Shader = renderer->GetShader("imgui"),
			.Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
			.Width = 64u,
			.Height = 64u,
			.CullMode = nvrhi::RasterCullMode::None,
			.BlendState = blendState,
		};

		const auto pipeline = CreateRef<Pipeline>(pipelineSpec);
		CHECK(pipeline->GetPipeline() != nullptr);
	}
}