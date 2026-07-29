#include "pch.h"
#include "Renderer/Framebuffer.h"

#include "Core/Application.h"

namespace Eppo
{
    Framebuffer::Framebuffer(FramebufferSpecification spec)
        : m_Specification(std::move(spec))
    {
        const auto& app = Application::Get();
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();

        if (m_Specification.Width == 0 || m_Specification.Height == 0)
        {
            // No size information, use window size
            m_Width = app.GetWindow()->GetWidth();
            m_Height = app.GetWindow()->GetHeight();
        }
        else
        {
            m_Width = m_Specification.Width;
            m_Height = m_Specification.Height;
        }

        // Create images if necessary
        nvrhi::FramebufferDesc framebufferDesc{};

        if (m_Specification.SwapchainTarget)
        {
            EP_ASSERT(m_Specification.SwapchainImage != nullptr, "SwapchainTarget is true on framebuffer but no swapchain image provided!");
            m_Images.emplace_back(m_Specification.SwapchainImage);
            framebufferDesc.addColorAttachment(m_Specification.SwapchainImage->GetTexture());
        }
        else
        {
            CreateImages(framebufferDesc);
        }

        m_Framebuffer = device->createFramebuffer(framebufferDesc);
    }

    auto Framebuffer::GetDepthImage() const -> const Ref<Image>&
    {
        for (const auto& image : m_Images)
        {
            if (image->IsDepthImage())
                return image;
        }

        EP_ASSERT(false, "Framebuffer has no depth image.");
        return m_Images.front();
    }

    auto Framebuffer::Resize(const uint32_t width, const uint32_t height) -> void
    {
        EP_PROFILE_FN("Framebuffer::Resize")

        if (m_Specification.SwapchainTarget || m_Specification.SwapchainImage)
        {
            Log::Warn("Trying to resize swapchain image through framebuffer. Please use Swapchain::Resize instead.");
            return;
        }

        const auto device = DeviceManager::Get()->GetDevice();

        m_Width = width;
        m_Height = height;

        nvrhi::FramebufferDesc framebufferDesc{};
        CreateImages(framebufferDesc);
        m_Framebuffer = device->createFramebuffer(framebufferDesc);
    }

    auto Framebuffer::CreateImages(nvrhi::FramebufferDesc& desc) -> void
    {
        uint32_t attachmentIndex = 0;
        m_Images.resize(m_Specification.Attachments.Attachments.size());

        for (const auto& attachment : m_Specification.Attachments.Attachments)
        {
            ImageSpecification spec{
                .ImageFormat = attachment.ImageFormat,
                .Width = m_Width,
                .Height = m_Height,
                .IsRenderTarget = true,
                .DebugName = std::format("{} Image {}", m_Specification.DebugName, attachmentIndex),
            };

            m_Images[attachmentIndex] = CreateRef<Image>(spec);
            attachmentIndex++;
        }

        // Dynamically created attachments target the whole texture (mip 0).
        for (const auto& image : m_Images)
        {
            if (image->IsDepthImage())
                desc.setDepthAttachment(image->GetTexture());
            else
                desc.addColorAttachment(image->GetTexture());
        }

        // The existing image is appended last, targeting a single mip with every array
        // slice attached — so a cubemap binds all 6 faces as one layered render target.
        if (const auto& existing = m_Specification.ExistingImage; existing.Image)
        {
            m_Images.emplace_back(existing.Image);

            const uint32_t arraySize = existing.Image->GetTexture()->getDesc().arraySize;
            const nvrhi::TextureSubresourceSet subresources(existing.MipLevel, 1, 0, arraySize);
            if (existing.Image->IsDepthImage())
                desc.setDepthAttachment(existing.Image->GetTexture(), subresources);
            else
                desc.addColorAttachment(existing.Image->GetTexture(), subresources);
        }
    }
}
