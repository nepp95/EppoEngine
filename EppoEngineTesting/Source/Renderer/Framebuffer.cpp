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

    TEST(Framebuffer_DepthOnlyTargetHasOneD32Attachment)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 2048u,
            .Height = 2048u,
            .Attachments = { nvrhi::Format::D32 },
            .DebugName = "Framebuffer shadow depth test",
        });

        REQUIRE CHECK(framebuffer->GetFramebuffer() != nullptr);
        REQUIRE CHECK(framebuffer->GetDepthImage() != nullptr);
        CHECK_EQUAL(2048u, framebuffer->GetWidth());
        CHECK_EQUAL(2048u, framebuffer->GetHeight());
        CHECK(framebuffer->GetDepthImage()->GetFormat() == nvrhi::Format::D32);

        const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
        CHECK(desc.colorAttachments.empty());
        CHECK(desc.depthAttachment.texture == framebuffer->GetDepthImage()->GetTexture());
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
            .Attachments = { FramebufferTextureSpecification(cubemap) },
            .DebugName = "Framebuffer existing cubemap target",
        });

        CHECK(framebuffer->GetFinalImage() == cubemap);

        // The supplied image is attached; the caller selects mip zero across all six faces.
        const nvrhi::FramebufferHandle handle = framebuffer->GetFramebuffer(nvrhi::TextureSubresourceSet(0, 1, 0, 6));
        REQUIRE CHECK(handle != nullptr);

        const nvrhi::FramebufferDesc& desc = handle->getDesc();
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

        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 128u,
            .Height = 128u,
            .Attachments = { FramebufferTextureSpecification(cubemap) },
            .DebugName = "Framebuffer mip cubemap target",
        });

        // The prefilter bake renders mip 2 (32^2) into the layered cube.
        const nvrhi::FramebufferHandle handle = framebuffer->GetFramebuffer(nvrhi::TextureSubresourceSet(2, 1, 0, 6));
        REQUIRE CHECK(handle != nullptr);

        const nvrhi::FramebufferDesc& desc = handle->getDesc();
        REQUIRE CHECK_EQUAL(1u, desc.colorAttachments.size());
        const nvrhi::FramebufferAttachment& attachment = desc.colorAttachments.front();
        CHECK(attachment.texture == cubemap->GetTexture());
        CHECK_EQUAL(2u, attachment.subresources.baseMipLevel);
        CHECK_EQUAL(1u, attachment.subresources.numMipLevels);
        CHECK_EQUAL(0u, attachment.subresources.baseArraySlice);
        CHECK_EQUAL(6u, attachment.subresources.numArraySlices);

        // Mip 2 of a 128^2 cube is 32^2; the handle carries the selected mip extent.
        CHECK_EQUAL(32u, handle->getFramebufferInfo().width);
        CHECK_EQUAL(32u, handle->getFramebufferInfo().height);
    }

    TEST(Framebuffer_MixedOwnedAndSuppliedAttachmentsKeepListOrder)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Ref<Image> supplied = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = 32u,
            .Height = 32u,
            .IsRenderTarget = true,
            .DebugName = "Framebuffer supplied image",
        });

        // An owned attachment declared first, a supplied image second: attachments keep list order.
        const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = 32u,
            .Height = 32u,
            .Attachments = { nvrhi::Format::RGBA8_UNORM, FramebufferTextureSpecification(supplied) },
            .DebugName = "Framebuffer mixed attachment ordering",
        });

        // The owned attachment is index zero; the supplied one follows.
        CHECK(framebuffer->GetFinalImage() != supplied);
        CHECK(framebuffer->GetFinalImage()->GetFormat() == nvrhi::Format::RGBA8_UNORM);

        const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
        REQUIRE CHECK_EQUAL(2u, desc.colorAttachments.size());
        CHECK(desc.colorAttachments.front().texture != supplied->GetTexture());
        CHECK(desc.colorAttachments.back().texture == supplied->GetTexture());
    }
}
