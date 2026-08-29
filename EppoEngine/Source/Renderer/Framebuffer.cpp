#include "pch.h"
#include "Renderer/Framebuffer.h"

#include "Core/Application.h"

namespace Eppo
{
    Framebuffer::Framebuffer(FramebufferSpecification spec)
        : m_Specification(std::move(spec))
    {
        const auto& app = Application::Get();

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

        CreateImages();
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

    auto Framebuffer::GetImage(const uint32_t index) const -> const Ref<Image>&
    {
        EP_ASSERT(index < m_Images.size());
        return m_Images.at(index);
    }

    auto Framebuffer::GetFramebuffer(const nvrhi::TextureSubresourceSet& subresources) const -> nvrhi::FramebufferHandle
    {
        EP_ASSERT(!m_Images.empty(), "Framebuffer has no attachments.");

        // Canonicalize the range against the attachment layout so equivalent requests hit the same cache entry.
        const auto resolved = subresources.resolve(m_Images.front()->GetTexture()->getDesc(), false);
        EP_ASSERT(resolved.numMipLevels == 1, "A framebuffer attachment must select exactly one mip.");

        if (const auto it = m_FramebufferCache.find(resolved); it != m_FramebufferCache.end())
            return it->second;

        const auto handle = BuildFramebuffer(resolved);
        m_FramebufferCache.emplace(resolved, handle);
        return handle;
    }

    auto Framebuffer::Resize(const uint32_t width, const uint32_t height) -> void
    {
        EP_PROFILE_FN("Framebuffer::Resize")

        if (m_Specification.SwapchainTarget || m_Specification.SwapchainImage)
        {
            Log::Warn("Trying to resize swapchain image through framebuffer. Please use Swapchain::Resize instead.");
            return;
        }

        m_Width = width;
        m_Height = height;

        // Cached handles reference the old owned images; drop them and rebuild the owned attachments at the new extent.
        m_FramebufferCache.clear();
        CreateImages();
    }

    auto Framebuffer::CreateImages() -> void
    {
        m_Images.clear();

        if (m_Specification.SwapchainTarget)
        {
            EP_ASSERT(m_Specification.SwapchainImage != nullptr, "SwapchainTarget is true on framebuffer but no swapchain image provided!");
            m_Images.emplace_back(m_Specification.SwapchainImage);
            return;
        }

        uint32_t attachmentIndex = 0;
        for (const auto& attachment : m_Specification.Attachments.Attachments)
        {
            // A supplied image is retained as-is (IBL/cubemap targets); a null one is owned and sized to this framebuffer.
            if (attachment.Image)
            {
                m_Images.emplace_back(attachment.Image);
            }
            else
            {
                const uint32_t mipLevels = glm::min(attachment.MaxMipLevels, Image::CalculateMipLevels(m_Width, m_Height));
                const ImageSpecification spec{
                    .ImageFormat = attachment.ImageFormat,
                    .Width = m_Width,
                    .Height = m_Height,
                    .MipLevels = mipLevels,
                    .IsRenderTarget = true,
                    .DebugName = std::format("{} Image {}", m_Specification.DebugName, attachmentIndex),
                };

                m_Images.emplace_back(Image::Create(spec));
            }

            attachmentIndex++;
        }
    }

    auto Framebuffer::BuildFramebuffer(const nvrhi::TextureSubresourceSet& subresources) const -> nvrhi::FramebufferHandle
    {
        const auto device = DeviceManager::Get()->GetDevice();

        nvrhi::FramebufferDesc desc{};
        for (const auto& image : m_Images)
        {
            if (image->IsDepthImage())
                desc.setDepthAttachment(image->GetTexture(), subresources);
            else
                desc.addColorAttachment(image->GetTexture(), subresources);
        }

        return device->createFramebuffer(desc);
    }
}
