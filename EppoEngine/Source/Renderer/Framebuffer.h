#pragma once

#include "Renderer/Image.h"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>

namespace Eppo
{
    struct FramebufferTextureSpecification
    {
        FramebufferTextureSpecification() = default;
        FramebufferTextureSpecification(const nvrhi::Format format, const uint32_t maxMipLevels = 1)
            : ImageFormat(format), MaxMipLevels(maxMipLevels)
        {}
        explicit FramebufferTextureSpecification(const Ref<Image>& image)
            : ImageFormat(image->GetFormat()), Image(image)
        {}

        nvrhi::Format ImageFormat = nvrhi::Format::UNKNOWN;

        // When set, the framebuffer renders into this existing image and ignores MaxMipLevels.
        // When null, the framebuffer owns a created image with min(MaxMipLevels, valid mip count) levels.
        Ref<Image> Image = nullptr;
        uint32_t MaxMipLevels = 1;
    };

    struct FramebufferAttachmentSpecification
    {
        FramebufferAttachmentSpecification() = default;
        FramebufferAttachmentSpecification(const std::initializer_list<FramebufferTextureSpecification>& attachments)
            : Attachments(attachments)
        {}

        std::vector<FramebufferTextureSpecification> Attachments;
    };

    struct FramebufferSpecification
    {
        uint32_t Width = 0;
        uint32_t Height = 0;

        FramebufferAttachmentSpecification Attachments;
        bool SwapchainTarget = false;
        Ref<Image> SwapchainImage = nullptr;

        std::string DebugName;
    };

    class Framebuffer
    {
    public:
        explicit Framebuffer(FramebufferSpecification spec);

        auto Resize(uint32_t width, uint32_t height) -> void;

        // Returns the NVRHI framebuffer handle for the requested subresource range, building and caching it on first request.
        // The range must resolve to exactly one mip; different mips of one image yield different handles with the same pipeline
        // compatibility.
        [[nodiscard]] auto GetFramebuffer(const nvrhi::TextureSubresourceSet& subresources = nvrhi::TextureSubresourceSet(0, 1, 0, 1)) const
            -> nvrhi::FramebufferHandle;
        [[nodiscard]] auto GetFinalImage() const -> const Ref<Image>& { return m_Images.at(0); }
        [[nodiscard]] auto GetDepthImage() const -> const Ref<Image>&;

        [[nodiscard]] constexpr auto GetSpecification() const -> const FramebufferSpecification& { return m_Specification; }
        [[nodiscard]] constexpr auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] constexpr auto GetHeight() const -> uint32_t { return m_Height; }

    private:
        auto CreateImages() -> void;
        [[nodiscard]] auto BuildFramebuffer(const nvrhi::TextureSubresourceSet& subresources) const -> nvrhi::FramebufferHandle;

    private:
        FramebufferSpecification m_Specification;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        std::vector<Ref<Image>> m_Images;
        mutable std::unordered_map<nvrhi::TextureSubresourceSet, nvrhi::FramebufferHandle> m_FramebufferCache;
    };
}
