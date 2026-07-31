#include "pch.h"
#include "Renderer/Image.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/Renderer.h"

#include <stb_image.h>

namespace Eppo
{
    Image::Image(const ImageSpecification& spec, const nvrhi::CommandListHandle& cmdList)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        const nvrhi::TextureDesc textureDesc{
            .width = m_Width,
            .height = m_Height,
            .arraySize = spec.IsCubemap ? 6u : 1u,
            .mipLevels = spec.MipLevels,
            .format = spec.ImageFormat,
            .dimension = spec.IsCubemap ? nvrhi::TextureDimension::TextureCube : nvrhi::TextureDimension::Texture2D,
            .debugName = spec.DebugName,
            .isRenderTarget = spec.IsRenderTarget,
            .initialState = spec.InitialState,
            .keepInitialState = spec.AutomaticStateTracking,
        };

        m_Texture = device->createTexture(textureDesc);
        m_MipLevels = m_Texture->getDesc().mipLevels;
        m_Stride = GetStride(m_Texture->getDesc().format);
    }

    Image::Image(const ImageSpecification& spec, void* existingImage)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        const nvrhi::TextureDesc textureDesc{
            .width = m_Width,
            .height = m_Height,
            .format = spec.ImageFormat,
            .debugName = spec.DebugName,
            .isRenderTarget = spec.IsRenderTarget,
            .initialState = spec.InitialState,
            .keepInitialState = spec.AutomaticStateTracking,
        };

        if (dm->GetParams().API == RendererAPI::Vulkan)
            m_Texture = device->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(existingImage), textureDesc);
        else
            EP_ASSERT(false);

        m_MipLevels = m_Texture->getDesc().mipLevels;
        m_Stride = GetStride(m_Texture->getDesc().format);
    }

    Image::Image(const ImageSpecification& spec, const ImageSource& source, const nvrhi::CommandListHandle& cmdList)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        EP_ASSERT(!spec.IsCubemap);

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = cmdList ? cmdList : device->createCommandList();

        uint32_t channels = 0;
        bool isHdr = false;
        auto* imageData = DecodeImageData(source, channels, isHdr);

        // Decoding is the authority on size and (for an unspecified format) format: .hdr -> float,
        // else sRGB. Fold both back into the spec so it stays the single source for the texture desc.
        m_Specification.Width = m_Width;
        m_Specification.Height = m_Height;
        if (m_Specification.ImageFormat == nvrhi::Format::UNKNOWN)
            m_Specification.ImageFormat = SelectFormat(channels, isHdr);

        const nvrhi::TextureDesc textureDesc{
            .width = m_Specification.Width,
            .height = m_Specification.Height,
            .format = m_Specification.ImageFormat,
            .debugName = m_Specification.DebugName,
            .isRenderTarget = m_Specification.IsRenderTarget,
            .initialState = m_Specification.InitialState,
            .keepInitialState = m_Specification.AutomaticStateTracking,
        };

        m_Texture = device->createTexture(textureDesc);
        m_MipLevels = m_Texture->getDesc().mipLevels;
        m_Stride = GetStride(m_Texture->getDesc().format);

        if (!cmdList)
            cmd->open();

        cmd->beginTrackingTextureState(m_Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
        cmd->writeTexture(m_Texture, 0, 0, imageData, m_Stride);
        cmd->setPermanentTextureState(m_Texture, nvrhi::ResourceStates::ShaderResource);
        cmd->commitBarriers();

        if (!cmdList)
        {
            cmd->close();
            device->executeCommandList(cmd);
        }

        // Both the path and in-memory decode paths allocate through stb, so free either.
        stbi_image_free(imageData);
    }

    auto Image::SetData(const void* data, uint64_t size, const nvrhi::CommandListHandle& cmdList) -> void
    {
        EP_ASSERT(m_Stride > 0);

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = cmdList ? cmdList : device->createCommandList({ .queueType = nvrhi::CommandQueue::Copy });

        if (!cmdList)
            cmd->open();

        cmdList->writeTexture(m_Texture, 0, 0, data, m_Stride);

        if (!cmdList)
        {
            cmd->close();
            device->executeCommandList(cmd, nvrhi::CommandQueue::Copy);
        }
    }

    auto Image::DecodeToRGBA8(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight) -> Buffer
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            Log::Error("Failed to decode image '{}': {}", path, stbi_failure_reason());
            outWidth = 0;
            outHeight = 0;
            return {};
        }

        outWidth = static_cast<uint32_t>(width);
        outHeight = static_cast<uint32_t>(height);

        // Copy into an engine-owned Buffer so the caller frees with delete[] (via
        // Buffer::Release) rather than needing stbi_image_free.
        const Buffer buffer = Buffer::Copy(pixels, static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4);
        stbi_image_free(pixels);
        return buffer;
    }

    auto Image::GetMipWidth(const uint32_t mipLevel) const -> uint32_t
    {
        EP_ASSERT(mipLevel < m_MipLevels);
        return glm::max(1u, m_Width >> mipLevel);
    }

    auto Image::GetMipHeight(const uint32_t mipLevel) const -> uint32_t
    {
        EP_ASSERT(mipLevel < m_MipLevels);
        return glm::max(1u, m_Height >> mipLevel);
    }

    auto Image::IsDepthImage() const -> bool
    {
        return m_Specification.ImageFormat == nvrhi::Format::D16 || m_Specification.ImageFormat == nvrhi::Format::D24S8 ||
            m_Specification.ImageFormat == nvrhi::Format::D32 || m_Specification.ImageFormat == nvrhi::Format::D32S8;
    }

    auto Image::GetBindlessIndex(const nvrhi::TextureSubresourceSet& subresources) -> uint32_t
    {
        const auto resolved = subresources.resolve(m_Texture->getDesc(), false);

        auto it = m_BindlessHandles.find(resolved);
        if (it == m_BindlessHandles.end())
        {
            const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
            it = m_BindlessHandles.emplace(resolved, CreateRef<BindlessHandle>(descriptorManager->Register(shared_from_this(), resolved))).first;
        }

        return it->second->Index;
    }

    auto Image::CalculateMipLevels(const uint32_t width, const uint32_t height) -> uint32_t
    {
        EP_ASSERT(width > 0 && height > 0);
        const uint32_t mipLevels =
            1 + static_cast<uint32_t>(glm::floor(glm::log2(glm::max(static_cast<float>(width), static_cast<float>(height)))));
        EP_ASSERT(mipLevels > 0);
        return mipLevels;
    }

    auto Image::DecodeImageData(const ImageSource& source, uint32_t& outChannels, bool& outIsHdr) -> void*
    {
        const auto* path = std::get_if<std::filesystem::path>(&source);
        const auto* buffer = std::get_if<Buffer>(&source);
        EP_ASSERT(path || buffer);

        int width = 0;
        int height = 0;
        int channels = 0;
        void* decodedData = nullptr;

        if (buffer)
            stbi_info_from_memory(buffer->Data, static_cast<int>(buffer->Size), &width, &height, &channels);
        else if (path)
            stbi_info(path->string().c_str(), &width, &height, &channels);

        m_Width = width;
        m_Height = height;

        bool isHdr = false;
        if (buffer)
            isHdr = stbi_is_hdr_from_memory(buffer->Data, static_cast<int>(buffer->Size));
        else if (path)
            isHdr = stbi_is_hdr(path->string().c_str());

        if (isHdr)
        {
            if (buffer)
                decodedData =
                    stbi_loadf_from_memory(buffer->Data, static_cast<int>(buffer->Size), &width, &height, &channels, STBI_rgb_alpha);
            else if (path)
                decodedData = stbi_loadf(path->string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
            channels = 4;
        }
        else
        {
            if (buffer)
                decodedData =
                    stbi_load_from_memory(buffer->Data, static_cast<int>(buffer->Size), &width, &height, &channels, STBI_rgb_alpha);
            else if (path)
                decodedData = stbi_load(path->string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
            channels = 4;
        }

        EP_ASSERT(decodedData);

        outChannels = static_cast<uint32_t>(channels);
        outIsHdr = isHdr;
        return decodedData;
    }

    auto Image::SelectFormat(const uint32_t channels, const bool isHdr) -> nvrhi::Format
    {
        auto format = nvrhi::Format::UNKNOWN;

        if (isHdr)
        {
            if (channels == 3 || channels == 4)
                format = nvrhi::Format::RGBA32_FLOAT;
        }
        else
        {
            if (channels == 3 || channels == 4)
                format = nvrhi::Format::SRGBA8_UNORM;
        }

        EP_ASSERT(format != nvrhi::Format::UNKNOWN);
        return format;
    }

    constexpr auto Image::GetStride(const nvrhi::Format format) const -> uint32_t
    {
        switch (format)
        {
            case nvrhi::Format::R8_UINT:
            case nvrhi::Format::R8_SINT:
            case nvrhi::Format::R8_UNORM:
            case nvrhi::Format::R8_SNORM:
                return m_Width;

            case nvrhi::Format::R16_UINT:
            case nvrhi::Format::R16_SINT:
            case nvrhi::Format::R16_UNORM:
            case nvrhi::Format::R16_SNORM:
            case nvrhi::Format::R16_FLOAT:
            case nvrhi::Format::RG8_UINT:
            case nvrhi::Format::RG8_SINT:
            case nvrhi::Format::RG8_UNORM:
            case nvrhi::Format::RG8_SNORM:
                return m_Width * 2;

            case nvrhi::Format::R32_UINT:
            case nvrhi::Format::R32_SINT:
            case nvrhi::Format::R32_FLOAT:
            case nvrhi::Format::RG16_UINT:
            case nvrhi::Format::RG16_SINT:
            case nvrhi::Format::RG16_UNORM:
            case nvrhi::Format::RG16_SNORM:
            case nvrhi::Format::RG16_FLOAT:
            case nvrhi::Format::RGBA8_UINT:
            case nvrhi::Format::RGBA8_SINT:
            case nvrhi::Format::RGBA8_UNORM:
            case nvrhi::Format::RGBA8_SNORM:
            case nvrhi::Format::SRGBA8_UNORM:
            case nvrhi::Format::BGRA8_UNORM:
            case nvrhi::Format::SBGRA8_UNORM:
            case nvrhi::Format::R11G11B10_FLOAT:
            case nvrhi::Format::R10G10B10A2_UNORM:
                return m_Width * 4;

            case nvrhi::Format::RG32_UINT:
            case nvrhi::Format::RG32_SINT:
            case nvrhi::Format::RG32_FLOAT:
            case nvrhi::Format::RGBA16_UINT:
            case nvrhi::Format::RGBA16_SINT:
            case nvrhi::Format::RGBA16_UNORM:
            case nvrhi::Format::RGBA16_SNORM:
            case nvrhi::Format::RGBA16_FLOAT:
                return m_Width * 8;

            case nvrhi::Format::RGBA32_UINT:
            case nvrhi::Format::RGBA32_SINT:
            case nvrhi::Format::RGBA32_FLOAT:
                return m_Width * 16;

            default:
                return 0;
        }
    }
}
