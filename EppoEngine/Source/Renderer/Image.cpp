#include "pch.h"
#include "Renderer/Image.h"

#include "Renderer/DeviceManager.h"

#include <stb_image.h>

namespace Eppo
{
    Image::Image(const ImageSpecification& spec, const nvrhi::CommandListHandle& cmdList)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();

        nvrhi::TextureDesc textureDesc{
            .width = m_Width,
            .height = m_Height,
            .format = spec.ImageFormat,
            .debugName = spec.DebugName,
            .isRenderTarget = spec.IsRenderTarget,
            .initialState = spec.InitialState,
            .keepInitialState = spec.AutomaticStateTracking,
        };

        m_Texture = device->createTexture(textureDesc);
        m_Stride = GetStride(m_Texture->getDesc().format);
    }

    Image::Image(const ImageSpecification& spec, void* existingImage)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::TextureDesc textureDesc{
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

        m_Stride = GetStride(m_Texture->getDesc().format);
    }

    Image::Image(const ImageSpecification& spec, ImageSource source, const nvrhi::CommandListHandle& cmdList)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_PROFILE_FN("Image::Image")

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = cmdList ? cmdList : device->createCommandList();
        auto* imageData = DecodeImageData(source);

        nvrhi::TextureDesc textureDesc{
            .width = m_Width,
            .height = m_Height,
            .format = spec.ImageFormat,
            .debugName = spec.DebugName,
            .isRenderTarget = spec.IsRenderTarget,
            .initialState = spec.InitialState,
            .keepInitialState = spec.AutomaticStateTracking,
        };

        m_Texture = device->createTexture(textureDesc);
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

        if (std::get_if<std::filesystem::path>(&source))
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
        Buffer buffer = Buffer::Copy(pixels, static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4);
        stbi_image_free(pixels);
        return buffer;
    }

    auto Image::IsDepthImage() const -> bool
    {
        return m_Specification.ImageFormat == nvrhi::Format::D16 || m_Specification.ImageFormat == nvrhi::Format::D24S8 ||
            m_Specification.ImageFormat == nvrhi::Format::D32 || m_Specification.ImageFormat == nvrhi::Format::D32S8;
    }

    auto Image::DecodeImageData(const ImageSource& source) -> void*
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
            stbi_is_hdr_from_memory(buffer->Data, static_cast<int>(buffer->Size));
        else if (path)
            stbi_is_hdr(path->string().c_str());

        if (isHdr)
        {
            EP_ASSERT(false);
        }
        else
        {
            if (channels == 3 || channels == 4)
            {
                if (buffer)
                    decodedData =
                        stbi_load_from_memory(buffer->Data, static_cast<int>(buffer->Size), &width, &height, &channels, STBI_rgb_alpha);
                else if (path)
                    decodedData = stbi_load(path->string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
                channels = 4;
            }
            else
            {
                if (buffer)
                    decodedData = stbi_load_from_memory(buffer->Data, static_cast<int>(buffer->Size), &width, &height, &channels, 0);
                else if (path)
                    decodedData = stbi_load(path->string().c_str(), &width, &height, &channels, 0);
            }
        }

        EP_ASSERT(decodedData);
        return decodedData;
    }

    auto Image::SelectFormat(uint32_t channels, bool isHdr) -> nvrhi::Format
    {
        nvrhi::Format format = nvrhi::Format::UNKNOWN;

        if (isHdr)
        {
            if (channels == 3 || channels == 4)
                format = nvrhi::Format::RGBA16_FLOAT;
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