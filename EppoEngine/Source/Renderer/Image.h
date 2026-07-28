#pragma once

#include <nvrhi/nvrhi.h>

#include <variant>

namespace Eppo
{
    using ImageSource = std::variant<std::filesystem::path, Buffer>;

    struct ImageSpecification
    {
        nvrhi::Format ImageFormat = nvrhi::Format::UNKNOWN;
        uint32_t Width = 0;
        uint32_t Height = 0;

        bool IsRenderTarget = false;
        bool AutomaticStateTracking = true;
        nvrhi::ResourceStates InitialState = nvrhi::ResourceStates::ShaderResource;

        std::string DebugName = "Image";
    };

    class Image
    {
    public:
        Image(const ImageSpecification& spec, void* ExistingImage);
        Image(const ImageSpecification& spec, ImageSource source, const nvrhi::CommandListHandle& cmdList = nullptr);
        Image(const ImageSpecification& spec, const nvrhi::CommandListHandle& cmdList = nullptr);
        ~Image() = default;

        auto SetData(const void* data, uint64_t size, const nvrhi::CommandListHandle& cmdList = nullptr) -> void;

        // Decode an image file into a tightly-packed, 4-channel RGBA8 pixel buffer on
        // the CPU. Ownership of the returned buffer transfers to the caller (call
        // Release()). Returns an empty buffer on failure (logged). Keeps stb_image
        // usage in one place for callers that need raw pixels rather than a GPU
        // texture (e.g. the GLFW window icon).
        [[nodiscard]] static auto DecodeToRGBA8(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight) -> Buffer;

        [[nodiscard]] auto GetTexture() const -> nvrhi::TextureHandle { return m_Texture; }
        [[nodiscard]] constexpr auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] constexpr auto GetHeight() const -> uint32_t { return m_Height; }
        [[nodiscard]] auto GetFormat() const -> nvrhi::Format { return m_Specification.ImageFormat; }
        [[nodiscard]] auto IsDepthImage() const -> bool;

    private:
        [[nodiscard]] auto DecodeImageData(const ImageSource& source) -> void*;
        auto SelectFormat(uint32_t channels, bool isHdr = false) -> nvrhi::Format;
        [[nodiscard]] constexpr auto GetStride(nvrhi::Format format) const -> uint32_t;

    private:
        ImageSpecification m_Specification;
        nvrhi::TextureHandle m_Texture = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_Stride = 0;
    };
}
