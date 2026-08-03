#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/Framebuffer.h"

using namespace Eppo;

TEST(Renderer, Framebuffer_CreatedWithExplicitSize_HasMatchingDimensionsAndHandle)
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

	EXPECT_TRUE(framebuffer->GetFramebuffer());
	EXPECT_EQ(256, framebuffer->GetWidth());
	EXPECT_EQ(256, framebuffer->GetHeight());
    EP_REQUIRE(framebuffer->GetDepthImage() != nullptr);
    EXPECT_TRUE(framebuffer->GetDepthImage()->GetFormat() == nvrhi::Format::D32);
}

TEST(Renderer, Framebuffer_DepthOnlyTargetHasOneD32Attachment)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Framebuffer> framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
        .Width = 2048u,
        .Height = 2048u,
        .Attachments = { nvrhi::Format::D32 },
        .DebugName = "Framebuffer shadow depth test",
    });

    EP_REQUIRE(framebuffer->GetFramebuffer() != nullptr);
    EP_REQUIRE(framebuffer->GetDepthImage() != nullptr);
    EXPECT_EQ(2048u, framebuffer->GetWidth());
    EXPECT_EQ(2048u, framebuffer->GetHeight());
    EXPECT_TRUE(framebuffer->GetDepthImage()->GetFormat() == nvrhi::Format::D32);

    const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
    EXPECT_TRUE(desc.colorAttachments.empty());
    EXPECT_TRUE(desc.depthAttachment.texture == framebuffer->GetDepthImage()->GetTexture());
}

TEST(Renderer, Framebuffer_ExistingCubemapIsUsedAsColorAttachment)
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

    EXPECT_TRUE(framebuffer->GetFinalImage() == cubemap);

    // The supplied image is attached; the caller selects mip zero across all six faces.
    const nvrhi::FramebufferHandle handle = framebuffer->GetFramebuffer(nvrhi::TextureSubresourceSet(0, 1, 0, 6));
    EP_REQUIRE(handle != nullptr);

    const nvrhi::FramebufferDesc& desc = handle->getDesc();
    EP_REQUIRE_EQ(1u, desc.colorAttachments.size());
    EXPECT_TRUE(desc.colorAttachments.front().texture == cubemap->GetTexture());
}

TEST(Renderer, Framebuffer_ExistingCubemapMipTarget_AttachesChosenMipAndAllFaces)
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
    EP_REQUIRE(handle != nullptr);

    const nvrhi::FramebufferDesc& desc = handle->getDesc();
    EP_REQUIRE_EQ(1u, desc.colorAttachments.size());
    const nvrhi::FramebufferAttachment& attachment = desc.colorAttachments.front();
    EXPECT_TRUE(attachment.texture == cubemap->GetTexture());
    EXPECT_EQ(2u, attachment.subresources.baseMipLevel);
    EXPECT_EQ(1u, attachment.subresources.numMipLevels);
    EXPECT_EQ(0u, attachment.subresources.baseArraySlice);
    EXPECT_EQ(6u, attachment.subresources.numArraySlices);

    // Mip 2 of a 128^2 cube is 32^2; the handle carries the selected mip extent.
    EXPECT_EQ(32u, handle->getFramebufferInfo().width);
    EXPECT_EQ(32u, handle->getFramebufferInfo().height);
}

TEST(Renderer, Framebuffer_MixedOwnedAndSuppliedAttachmentsKeepListOrder)
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
    EXPECT_TRUE(framebuffer->GetFinalImage() != supplied);
    EXPECT_TRUE(framebuffer->GetFinalImage()->GetFormat() == nvrhi::Format::RGBA8_UNORM);

    const nvrhi::FramebufferDesc& desc = framebuffer->GetFramebuffer()->getDesc();
    EP_REQUIRE_EQ(2u, desc.colorAttachments.size());
    EXPECT_TRUE(desc.colorAttachments.front().texture != supplied->GetTexture());
    EXPECT_TRUE(desc.colorAttachments.back().texture == supplied->GetTexture());
}
