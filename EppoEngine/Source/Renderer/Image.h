#pragma once

#include "Asset/Asset.h"

#include <nvrhi/nvrhi.h>

#include <variant>

namespace Eppo
{
    struct BindlessHandle;
    using ImageSource = std::variant<std::filesystem::path, Buffer>;

    struct ImageSpecification
    {
        nvrhi::Format ImageFormat = nvrhi::Format::UNKNOWN;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 1;

        bool IsCubemap = false;
        bool IsRenderTarget = false;
        bool AutomaticStateTracking = true;
        nvrhi::ResourceStates InitialState = nvrhi::ResourceStates::ShaderResource;

        std::string DebugName = "Image";
    };

    class Image : public Asset, public std::enable_shared_from_this<Image>
    {
    public:
        explicit Image(const ImageSpecification& spec, const nvrhi::CommandListHandle& cmdList = nullptr);
        explicit Image(const ImageSpecification& spec, void* ExistingImage);
        explicit Image(const ImageSpecification& spec, const ImageSource& source, const nvrhi::CommandListHandle& cmdList = nullptr);
        ~Image() = default;

        static auto GetStaticType() -> AssetType { return AssetType::Texture; }

        auto SetData(const void* data, uint64_t size, const nvrhi::CommandListHandle& cmdList = nullptr) -> void;

        // Decode an image file into a tightly-packed, 4-channel RGBA8 pixel buffer on
        // the CPU. Ownership of the returned buffer transfers to the caller (call Release()).
        [[nodiscard]] static auto DecodeToRGBA8(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight) -> Buffer;

        [[nodiscard]] auto GetTexture() const -> nvrhi::TextureHandle { return m_Texture; }
        [[nodiscard]] constexpr auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] constexpr auto GetHeight() const -> uint32_t { return m_Height; }
        [[nodiscard]] auto GetFormat() const -> nvrhi::Format { return m_Specification.ImageFormat; }
        [[nodiscard]] auto IsDepthImage() const -> bool;

        // Registers this image in the bindless resource heap on first call and returns its slot.
        [[nodiscard]] auto GetBindlessIndex() -> uint32_t;

    private:
        [[nodiscard]] auto DecodeImageData(const ImageSource& source, uint32_t& outChannels, bool& outIsHdr) -> void*;
        auto SelectFormat(uint32_t channels, bool isHdr = false) -> nvrhi::Format;
        [[nodiscard]] constexpr auto GetStride(nvrhi::Format format) const -> uint32_t;

    private:
        ImageSpecification m_Specification;
        nvrhi::TextureHandle m_Texture = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_Stride = 0;

        Ref<BindlessHandle> m_BindlessHandle = nullptr;
    };
}
