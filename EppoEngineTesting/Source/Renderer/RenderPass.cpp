#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Renderer.h"
#include "Renderer/Sampler.h"
#include "Renderer/Shader.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"

using namespace Eppo;

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

		const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

		const PipelineSpecification pipelineSpec{
			.Shader = renderer->GetShader("geometry"),
			.CullMode = nvrhi::RasterCullMode::Front,
			.DepthTestEnable = true,
			.DepthWriteEnable = true,
		};

		return CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo());
	}

    auto MakeCompositePipeline() -> Ref<Pipeline>
    {
        const auto& renderer = DeviceManager::Get()->GetRenderer();

        const FramebufferSpecification framebufferSpec{
            .Width = 256,
            .Height = 256,
            .Attachments = { nvrhi::Format::RGBA8_UNORM },
            .DebugName = "Framebuffer CompositeRenderPassTest",
        };

        const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

        const PipelineSpecification pipelineSpec{
            .Shader = renderer->GetShader("composite"),
            .CullMode = nvrhi::RasterCullMode::None,
        };

        return CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo());
    }

    auto MakeSharedBindingPipeline() -> Ref<Pipeline>
    {
        const auto shader = Shader::Create(ShaderSpecification{
            .Name = "SharedLogicalBindingRenderPassTest",
            .Source = R"(
struct Input
{
float3 Position : POSITION0;
};

struct Constants
{
float4x4 Transform;
};

ConstantBuffer<Constants> uConstants : register(b0, space0);
StructuredBuffer<float4x4> uTransforms : register(t0, space0);

float4 VSMain(Input input) : SV_Position
{
return mul(uConstants.Transform, mul(uTransforms[0], float4(input.Position, 1.0)));
}

float4 PSMain() : SV_Target
{
return float4(1.0, 1.0, 1.0, 1.0);
}
)",
        });

        const FramebufferSpecification framebufferSpec{
            .Width = 256,
            .Height = 256,
            .Attachments = { nvrhi::Format::RGBA8_UNORM },
            .DebugName = "Framebuffer SharedLogicalBindingRenderPassTest",
        };

        const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

        return CreateRef<Pipeline>(
            PipelineSpecification{
                .Shader = shader,
                .CullMode = nvrhi::RasterCullMode::None,
            },
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        );
    }

    auto SetGeometryInputs(
        RenderPass& pass,
        const Ref<UniformBuffer>& camera,
        const Ref<UniformBuffer>& lights,
        const Ref<UniformBuffer>& environment,
        const Ref<StorageBuffer>& instances
    ) -> void
    {
        const auto shadow = CreateRef<UniformBuffer>(4096, "TestCB Shadow");
        const auto drawData = CreateRef<StorageBuffer>(80, 80, "TestSB Draw Data");
        const auto materialData = CreateRef<StorageBuffer>(80, 80, "TestSB Material Data");
        const auto materialSampler = Sampler::Create();
        const auto ssao = CreateRef<UniformBuffer>(sizeof(glm::vec4) * 34, "TestCB Ssao");
        const auto ssaoTex = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA8_UNORM,
            .Width = 4,
            .Height = 4,
            .DebugName = "Image RenderPassTest Ssao",
        });
        const auto ssaoSampler = Sampler::Create();

        pass.SetInput(0, 0, materialSampler);
        pass.SetInput(0, 1, ssaoSampler);
        pass.SetInput(0, 0, instances);
        pass.SetInput(0, 1, shadow);
        pass.SetInput(0, 1, drawData);
        pass.SetInput(0, 2, camera);
        pass.SetInput(0, 2, materialData);
        pass.SetInput(0, 3, lights);
        pass.SetInput(0, 3, ssaoTex);
        pass.SetInput(0, 4, environment);
        pass.SetInput(0, 5, ssao);
    }

    [[nodiscard]] auto FindBinding(
        const nvrhi::BindingSetDesc& desc, const uint32_t slot, const nvrhi::ResourceType type
    ) -> const nvrhi::BindingSetItem*
    {
        const auto it = std::ranges::find_if(
            desc.bindings,
            [slot, type](const nvrhi::BindingSetItem& item) -> bool
            {
                return item.slot == slot && item.type == type;
            }
        );
        return it != desc.bindings.end() ? &*it : nullptr;
    }

    [[nodiscard]] auto MakeTestImage() -> Ref<Image>
    {
        return CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA8_UNORM,
            .Width = 4,
            .Height = 4,
            .DebugName = "Image RenderPassTest",
        });
    }
}

TEST(Renderer, RenderPass_ConstructionStoresSpecification)
{
	const RenderPass pass(RenderPassSpecification{
		.Name = "TestPass",
	    .Pipeline = MakeGeometryPipeline(),
		.ClearColorOnLoad = true,
		.ClearDepthOnLoad = false,
	});

	EXPECT_EQ(std::string("TestPass"), pass.GetName());
    EXPECT_TRUE(pass.GetSpecification().Pipeline);
	EXPECT_TRUE(pass.GetSpecification().ClearColorOnLoad);
	EXPECT_TRUE(!pass.GetSpecification().ClearDepthOnLoad);
}

TEST(Renderer, RenderPass_DefaultConstructionHasNoPipelineOrBindingSets)
{
	const RenderPass pass;

	EXPECT_TRUE(!pass.GetPipeline());
	EXPECT_TRUE(pass.GetBindingSets().empty());
}

TEST(Renderer, RenderPass_StatisticsAreOwnedAndMutable)
{
    RenderPass pass;
    pass.GetStatistics().DrawCalls = 3;
    pass.GetStatistics().Instances = 7;

    const auto& constPass = pass;
    EXPECT_EQ(3u, constPass.GetStatistics().DrawCalls);
    EXPECT_EQ(7u, constPass.GetStatistics().Instances);
}

TEST(Renderer, RenderPass_BakeMergesBoundAndBindlessSetsWithoutGaps)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const auto pipeline = MakeGeometryPipeline();
	const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
	const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
	const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
	const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Instances");

	RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = pipeline });
    SetGeometryInputs(pass, camera, lights, environment, instances);
	pass.Bake();

	const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
	const auto& bindingSets = pass.GetBindingSets();

	EXPECT_EQ(3u, static_cast<uint32_t>(bindingSets.size()));
	EXPECT_TRUE(bindingSets[0] != nullptr);
	EXPECT_TRUE(bindingSets[0]->getDesc() != nullptr);
	EXPECT_EQ(1ul, bindingSets[0]->GetRefCount());
	EXPECT_TRUE(bindingSets[1] == descriptorManager->GetResourceDT().Get());
	EXPECT_TRUE(bindingSets[2] == descriptorManager->GetSamplerDT().Get());
}

TEST(Renderer, RenderPass_BakeDerivesPushConstantsFromShaderReflection)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto pipeline = MakeGeometryPipeline();
    const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
    const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
    const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
    const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Instances");

    RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = pipeline });
    SetGeometryInputs(pass, camera, lights, environment, instances);
    pass.Bake();

    const auto& pushConstants = pipeline->GetSpecification().Shader->GetPushConstants();
    const auto* item = FindBinding(*pass.GetBindingSets().at(0)->getDesc(), pushConstants.Binding, nvrhi::ResourceType::PushConstants);

    EP_REQUIRE(item != nullptr);
    EXPECT_EQ(pushConstants.Size, item->range.byteSize);
}

TEST(Renderer, RenderPass_InputsPersistAcrossForcedRebake)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
    const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
    const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
    const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Instances");

    RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = MakeGeometryPipeline() });
    SetGeometryInputs(pass, camera, lights, environment, instances);
    pass.Bake();

    const nvrhi::BindingSetHandle firstSet = pass.GetBindingSets().at(0);
    const auto firstDesc = *firstSet->getDesc();
    pass.Invalidate();
    pass.Bake();

    EXPECT_EQ(3u, static_cast<uint32_t>(pass.GetBindingSets().size()));
    EXPECT_TRUE(firstSet.Get() != pass.GetBindingSets().at(0));
    EXPECT_TRUE(firstDesc == *pass.GetBindingSets().at(0)->getDesc());
}

TEST(Renderer, RenderPass_BakeRebuildsWhenStorageBufferGrows)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
    const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
    const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
    const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "TestSSBO Instances");

    RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = MakeGeometryPipeline() });
    SetGeometryInputs(pass, camera, lights, environment, instances);
    pass.Bake();

    const nvrhi::BindingSetHandle firstSet = pass.GetBindingSets().at(0);
    const std::array transforms{ glm::mat4(1.0f), glm::mat4(1.0f) };
    instances->SetData(transforms.data(), sizeof(transforms));
    pass.Bake();

    const auto* item = FindBinding(*pass.GetBindingSets().at(0)->getDesc(), 0, nvrhi::ResourceType::StructuredBuffer_SRV);
    EP_REQUIRE(item != nullptr);
    EXPECT_TRUE(firstSet.Get() != pass.GetBindingSets().at(0));
    EXPECT_TRUE(item->resourceHandle == instances->GetBuffer().Get());
}

TEST(Renderer, RenderPass_SetInputBuildsBufferItemsMatchingResourceType)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
    const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
    const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
    const auto instances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Instances");

    RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = MakeGeometryPipeline() });
    SetGeometryInputs(pass, camera, lights, environment, instances);
    pass.Bake();

    const auto& desc = *pass.GetBindingSets().at(0)->getDesc();
    EXPECT_TRUE(FindBinding(desc, 1, nvrhi::ResourceType::ConstantBuffer) != nullptr);
    EXPECT_TRUE(FindBinding(desc, 0, nvrhi::ResourceType::StructuredBuffer_SRV) != nullptr);
}

TEST(Renderer, RenderPass_SetInputBuildsTextureAndSamplerItemsMatchingResourceType)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto image = MakeTestImage();
    const auto sampler = Sampler::Create();
    RenderPass pass(RenderPassSpecification{ .Name = "Composite", .Pipeline = MakeCompositePipeline() });
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    pass.Bake();

    const auto& desc = *pass.GetBindingSets().at(0)->getDesc();
    EXPECT_TRUE(FindBinding(desc, 0, nvrhi::ResourceType::Texture_SRV) != nullptr);
    EXPECT_TRUE(FindBinding(desc, 0, nvrhi::ResourceType::Sampler) != nullptr);
}

TEST(Renderer, RenderPass_BakeIsNoOpWhenNothingChanged)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto image = MakeTestImage();
    const auto sampler = Sampler::Create();
    RenderPass pass(RenderPassSpecification{ .Name = "Composite", .Pipeline = MakeCompositePipeline() });
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    pass.Bake();

    const nvrhi::BindingSetHandle firstSet = pass.GetBindingSets().at(0);
    pass.Bake();

    EXPECT_TRUE(firstSet.Get() == pass.GetBindingSets().at(0));
}

TEST(Renderer, RenderPass_RepeatingSameInputDoesNotInvalidate)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto image = MakeTestImage();
    const auto sampler = Sampler::Create();
    RenderPass pass(RenderPassSpecification{ .Name = "Composite", .Pipeline = MakeCompositePipeline() });
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    pass.Bake();

    const nvrhi::BindingSetHandle firstSet = pass.GetBindingSets().at(0);
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    pass.Bake();

    EXPECT_TRUE(firstSet.Get() == pass.GetBindingSets().at(0));
}

TEST(Renderer, RenderPass_InvalidateForcesRebake)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto image = MakeTestImage();
    const auto sampler = Sampler::Create();
    RenderPass pass(RenderPassSpecification{ .Name = "Composite", .Pipeline = MakeCompositePipeline() });
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    pass.Bake();

    const nvrhi::BindingSetHandle firstSet = pass.GetBindingSets().at(0);
    pass.Invalidate();
    pass.Bake();

    EXPECT_TRUE(firstSet.Get() != pass.GetBindingSets().at(0));
}

TEST(Renderer, RenderPass_SetInputReplacesMatchingBindingAndType)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto camera = CreateRef<UniformBuffer>(4096, "TestCB Camera");
    const auto lights = CreateRef<UniformBuffer>(4096, "TestCB Lights");
    const auto environment = CreateRef<UniformBuffer>(4096, "TestCB Environment");
    const auto firstInstances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO First Instances");
    const auto secondInstances = CreateRef<StorageBuffer>(sizeof(glm::mat4), 4096, "TestSSBO Second Instances");

    RenderPass pass(RenderPassSpecification{ .Name = "Geometry", .Pipeline = MakeGeometryPipeline() });
    SetGeometryInputs(pass, camera, lights, environment, firstInstances);
    pass.SetInput(0, 0, secondInstances);
    pass.Bake();

    const auto& desc = *pass.GetBindingSets().at(0)->getDesc();
    const auto matchingItems = std::ranges::count_if(
        desc.bindings,
        [](const nvrhi::BindingSetItem& item) -> bool
        {
            return item.slot == 0 && item.type == nvrhi::ResourceType::StructuredBuffer_SRV;
        }
    );
    const auto* item = FindBinding(desc, 0, nvrhi::ResourceType::StructuredBuffer_SRV);

    EXPECT_EQ(1, matchingItems);
    EP_REQUIRE(item != nullptr);
    EXPECT_TRUE(item->resourceHandle == secondInstances->GetBuffer().Get());
}

TEST(Renderer, RenderPass_DistinctResourceTypesAtTheSameBindingCoexist)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto constants = CreateRef<UniformBuffer>(sizeof(glm::mat4), "TestCB Constants");
    const auto transforms = CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "TestSSBO Transforms");
    RenderPass pass(RenderPassSpecification{ .Name = "SharedLogicalBinding", .Pipeline = MakeSharedBindingPipeline() });
    pass.SetInput(0, 0, constants);
    pass.SetInput(0, 0, transforms);
    pass.Bake();

    const auto& desc = *pass.GetBindingSets().at(0)->getDesc();
    EXPECT_TRUE(FindBinding(desc, 0, nvrhi::ResourceType::ConstantBuffer) != nullptr);
    EXPECT_TRUE(FindBinding(desc, 0, nvrhi::ResourceType::StructuredBuffer_SRV) != nullptr);
}

TEST(Renderer, RenderPass_InputKeepsResourceAliveAfterCallerDropsRef)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    auto image = MakeTestImage();
    const WeakRef<Image> weakImage = image;
    const auto sampler = Sampler::Create();
    RenderPass pass(RenderPassSpecification{ .Name = "Composite", .Pipeline = MakeCompositePipeline() });
    pass.SetInput(0, 0, image);
    pass.SetInput(0, 0, sampler);
    image.reset();
    pass.Bake();

    EXPECT_TRUE(!weakImage.expired());
    EXPECT_TRUE(pass.GetBindingSets().at(0) != nullptr);
}
