#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Renderer.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"

using namespace Eppo;

SUITE(Renderer)
{
	namespace
	{
		auto MakeGeometryPipeline() -> Ref<Pipeline>
		{
			const auto& renderer = DeviceManager::Get()->GetRenderer();

			const FramebufferSpecification framebufferSpec{
				.Width = 256,
				.Height = 256,
				.Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
				.DebugName = "Framebuffer RenderPassTest",
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

			return CreateRef<Pipeline>(pipelineSpec);
		}
	}

	TEST(RenderPass_ConstructionStoresSpecification)
	{
		const RenderPass pass(RenderPassSpecification{
			.Name = "TestPass",
		    .Pipeline = MakeGeometryPipeline(),
			.ClearColorOnLoad = true,
			.ClearDepthOnLoad = false,
		});

		CHECK_EQUAL(std::string("TestPass"), pass.GetName());
	    CHECK(pass.GetSpecification().Pipeline);
		CHECK(pass.GetSpecification().ClearColorOnLoad);
		CHECK(!pass.GetSpecification().ClearDepthOnLoad);
	}

	TEST(RenderPass_DefaultConstructionHasNoPipelineOrBindingSets)
	{
		const RenderPass pass;

		CHECK(!pass.GetPipeline());
		CHECK(pass.GetBindingSets().empty());
	}

	TEST(RenderPass_BakeMergesBoundAndBindlessSetsWithoutGaps)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto pipeline = MakeGeometryPipeline();
		const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
		const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
		const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
		const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Instances");

		RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = pipeline });
		pass.DeclarePushConstants(0, pipeline->GetSpecification().Shader->GetPushConstants().Size);
		pass.SetInput(0, 1, camera->GetBuffer());
		pass.SetInput(0, 2, lights->GetBuffer());
		pass.SetInput(0, 3, environment->GetBuffer());
		pass.SetInput(0, 0, instances->GetBuffer());
		pass.Bake();

		const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
		const auto& bindingSets = pass.GetBindingSets();

		CHECK_EQUAL(3u, static_cast<uint32_t>(bindingSets.size()));
		CHECK(bindingSets[0] != nullptr);
		CHECK(bindingSets[0]->getDesc() != nullptr);
		CHECK_EQUAL(1ul, bindingSets[0]->GetRefCount());
		CHECK(bindingSets[1] == descriptorManager->GetResourceDT().Get());
		CHECK(bindingSets[2] == descriptorManager->GetSamplerDT().Get());
	}
}
