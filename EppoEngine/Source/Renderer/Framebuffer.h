#pragma once

#include "Renderer/Image.h"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>

namespace Eppo
{
    struct FramebufferTextureSpecification
    {
        FramebufferTextureSpecification() = default;
        FramebufferTextureSpecification(const nvrhi::Format format)
            : ImageFormat(format)
        {}

        nvrhi::Format ImageFormat = nvrhi::Format::UNKNOWN;
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
        Framebuffer(FramebufferSpecification spec);

        auto Resize(uint32_t width, uint32_t height) -> void;

        [[nodiscard]] auto GetFramebuffer() const -> nvrhi::FramebufferHandle { return m_Framebuffer; }
        [[nodiscard]] auto GetFinalImage() const -> const Ref<Image>& { return m_Images.at(0); }
        [[nodiscard]] auto GetDepthImage() const -> const Ref<Image>&;

        [[nodiscard]] constexpr auto GetSpecification() const -> const FramebufferSpecification& { return m_Specification; }
        [[nodiscard]] constexpr auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] constexpr auto GetHeight() const -> uint32_t { return m_Height; }

    private:
        auto CreateImages(nvrhi::FramebufferDesc& desc) -> void;

    private:
        FramebufferSpecification m_Specification;
        nvrhi::FramebufferHandle m_Framebuffer = nullptr;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        std::vector<Ref<Image>> m_Images;
    };
}
