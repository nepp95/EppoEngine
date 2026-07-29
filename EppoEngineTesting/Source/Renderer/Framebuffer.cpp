#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/Framebuffer.h"

using namespace Eppo;

SUITE(Renderer)
{
	TEST(Framebuffer_CreatedWithExplicitSize_HasMatchingDimensionsAndHandle)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const FramebufferSpecification spec{
			.Width = 256,
			.Height = 256,
			.Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
			.DebugName = "Framebuffer RendererTest",
		};

		const auto framebuffer = CreateRef<Framebuffer>(spec);

		CHECK(framebuffer->GetFramebuffer());
		CHECK_EQUAL(256, framebuffer->GetWidth());
		CHECK_EQUAL(256, framebuffer->GetHeight());
        REQUIRE CHECK(framebuffer->GetDepthImage() != nullptr);
        CHECK(framebuffer->GetDepthImage()->GetFormat() == nvrhi::Format::D32);
	}

    TEST(Framebuffer_ExistingCubemapIsUsedAsColorAttachment)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Ref<Image> cubemap = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = 64u,
            .Height = 64u,
            .MipLevels = 1u,
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "Framebuffer existing cubemap",
        });

        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 64u,
            .Height = 64u,
            .ExistingImage = { .Image = cubemap },
            .DebugName = "Framebuffer existing cubemap target",
        });

        REQUIRE CHECK(framebuffer->GetFramebuffer() != nullptr);
        CHECK(framebuffer->GetFinalImage() == cubemap);

        const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
        REQUIRE CHECK_EQUAL(1u, desc.colorAttachments.size());
        CHECK(desc.colorAttachments.front().texture == cubemap->GetTexture());
    }

    TEST(Framebuffer_ExistingCubemapMipTarget_AttachesChosenMipAndAllFaces)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Ref<Image> cubemap = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = 128u,
            .Height = 128u,
            .MipLevels = 5u,
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "Framebuffer mip cubemap",
        });

        // The prefilter bake renders mip 2 (32^2) into the layered cube.
        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 32u,
            .Height = 32u,
            .ExistingImage = { .Image = cubemap, .MipLevel = 2u },
            .DebugName = "Framebuffer mip cubemap target",
        });

        REQUIRE CHECK(framebuffer->GetFramebuffer() != nullptr);

        const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
        REQUIRE CHECK_EQUAL(1u, desc.colorAttachments.size());
        const nvrhi::FramebufferAttachment& attachment = desc.colorAttachments.front();
        CHECK(attachment.texture == cubemap->GetTexture());
        CHECK_EQUAL(2u, attachment.subresources.baseMipLevel);
        CHECK_EQUAL(1u, attachment.subresources.numMipLevels);
        CHECK_EQUAL(0u, attachment.subresources.baseArraySlice);
        CHECK_EQUAL(6u, attachment.subresources.numArraySlices);
    }

    TEST(Framebuffer_ExistingImageIsAppendedAfterDynamicAttachments)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Ref<Image> existing = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = 32u,
            .Height = 32u,
            .IsRenderTarget = true,
            .DebugName = "Framebuffer existing image",
        });

        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 32u,
            .Height = 32u,
            .Attachments = { nvrhi::Format::RGBA8_UNORM },
            .ExistingImage = { .Image = existing },
            .DebugName = "Framebuffer existing image ordering",
        });

        REQUIRE CHECK(framebuffer->GetFramebuffer() != nullptr);

        // The dynamically created attachment stays first; ExistingImage is appended last.
        CHECK(framebuffer->GetFinalImage() != existing);
        CHECK(framebuffer->GetFinalImage()->GetFormat() == nvrhi::Format::RGBA8_UNORM);

        const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
        REQUIRE CHECK_EQUAL(2u, desc.colorAttachments.size());
        CHECK(desc.colorAttachments.front().texture != existing->GetTexture());
        CHECK(desc.colorAttachments.back().texture == existing->GetTexture());
    }
}
