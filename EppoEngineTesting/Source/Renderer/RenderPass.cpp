#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

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

        auto MakeCompositePipeline() -> Ref<Pipeline>
        {
            const auto& renderer = DeviceManager::Get()->GetRenderer();

            const FramebufferSpecification framebufferSpec{
                .Width = 256,
                .Height = 256,
                .Attachments = { nvrhi::Format::RGBA8_UNORM },
                .DebugName = "Framebuffer CompositeRenderPassTest",
            };

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("composite"),
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = 256,
                .Height = 256,
                .CullMode = nvrhi::RasterCullMode::None,
            };

            return CreateRef<Pipeline>(pipelineSpec);
        }

        auto MakeSharedBindingPipeline() -> Ref<Pipeline>
        {
            const auto shader = Shader::Create(ShaderSpecification{
                .Name = "SharedLogicalBindingRenderPassTest",
                .Sources = {
                    { nvrhi::ShaderType::Vertex,
                      R"(
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

float4 Main(Input input) : SV_Position
{
    return mul(uConstants.Transform, mul(uTransforms[0], float4(input.Position, 1.0)));
}
)" },
                    { nvrhi::ShaderType::Pixel,
                      R"(
float4 Main() : SV_Target
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
)" },
                },
            });

            const FramebufferSpecification framebufferSpec{
                .Width = 256,
                .Height = 256,
                .Attachments = { nvrhi::Format::RGBA8_UNORM },
                .DebugName = "Framebuffer SharedLogicalBindingRenderPassTest",
            };

            return CreateRef<Pipeline>(PipelineSpecification{
                .Shader = shader,
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = 256,
                .Height = 256,
                .CullMode = nvrhi::RasterCullMode::None,
            });
        }

        auto SetGeometryInputs(
            RenderPass& pass,
            const Ref<UniformBuffer>& camera,
            const Ref<UniformBuffer>& lights,
            const Ref<UniformBuffer>& environment,
            const Ref<StorageBuffer>& instances
        ) -> void
        {
            pass.SetInput(0, 1, camera);
            pass.SetInput(0, 2, lights);
            pass.SetInput(0, 3, environment);
            pass.SetInput(0, 0, instances);
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

    TEST(RenderPass_StatisticsAreOwnedAndMutable)
    {
        RenderPass pass;
        pass.GetStatistics().DrawCalls = 3;
        pass.GetStatistics().Instances = 7;

        const auto& constPass = pass;
        CHECK_EQUAL(3u, constPass.GetStatistics().DrawCalls);
        CHECK_EQUAL(7u, constPass.GetStatistics().Instances);
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
        SetGeometryInputs(pass, camera, lights, environment, instances);
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

    TEST(RenderPass_BakeDerivesPushConstantsFromShaderReflection)
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

        REQUIRE CHECK(item != nullptr);
        CHECK_EQUAL(pushConstants.Size, item->range.byteSize);
    }

    TEST(RenderPass_InputsPersistAcrossForcedRebake)
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

        CHECK_EQUAL(3u, static_cast<uint32_t>(pass.GetBindingSets().size()));
        CHECK(firstSet.Get() != pass.GetBindingSets().at(0));
        CHECK(firstDesc == *pass.GetBindingSets().at(0)->getDesc());
    }

    TEST(RenderPass_BakeRebuildsWhenStorageBufferGrows)
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
        REQUIRE CHECK(item != nullptr);
        CHECK(firstSet.Get() != pass.GetBindingSets().at(0));
        CHECK(item->resourceHandle == instances->GetBuffer().Get());
    }

    TEST(RenderPass_SetInputBuildsBufferItemsMatchingResourceType)
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
        CHECK(FindBinding(desc, 1, nvrhi::ResourceType::ConstantBuffer) != nullptr);
        CHECK(FindBinding(desc, 0, nvrhi::ResourceType::StructuredBuffer_SRV) != nullptr);
    }

    TEST(RenderPass_SetInputBuildsTextureAndSamplerItemsMatchingResourceType)
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
        CHECK(FindBinding(desc, 0, nvrhi::ResourceType::Texture_SRV) != nullptr);
        CHECK(FindBinding(desc, 0, nvrhi::ResourceType::Sampler) != nullptr);
    }

    TEST(RenderPass_BakeIsNoOpWhenNothingChanged)
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

        CHECK(firstSet.Get() == pass.GetBindingSets().at(0));
    }

    TEST(RenderPass_RepeatingSameInputDoesNotInvalidate)
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

        CHECK(firstSet.Get() == pass.GetBindingSets().at(0));
    }

    TEST(RenderPass_InvalidateForcesRebake)
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

        CHECK(firstSet.Get() != pass.GetBindingSets().at(0));
    }

    TEST(RenderPass_SetInputReplacesMatchingBindingAndType)
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

        CHECK_EQUAL(1, matchingItems);
        REQUIRE CHECK(item != nullptr);
        CHECK(item->resourceHandle == secondInstances->GetBuffer().Get());
    }

    TEST(RenderPass_DistinctResourceTypesAtTheSameBindingCoexist)
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
        CHECK(FindBinding(desc, 0, nvrhi::ResourceType::ConstantBuffer) != nullptr);
        CHECK(FindBinding(desc, 0, nvrhi::ResourceType::StructuredBuffer_SRV) != nullptr);
    }

    TEST(RenderPass_InputKeepsResourceAliveAfterCallerDropsRef)
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

        CHECK(!weakImage.expired());
        CHECK(pass.GetBindingSets().at(0) != nullptr);
    }
}
