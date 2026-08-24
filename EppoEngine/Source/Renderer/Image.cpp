#include "pch.h"
#include "Renderer/Image.h"

#include "Core/Application.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/Renderer.h"

#include <stb_image.h>

namespace Eppo
{
    Image::Image(const ImageSpecification& spec)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
        EP_ASSERT(spec.ArraySize > 0);

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        auto dimension = nvrhi::TextureDimension::Texture2D;
        if (spec.IsCubemap)
            dimension = nvrhi::TextureDimension::TextureCube;
        if (!spec.IsCubemap && spec.ArraySize > 1)
            dimension = nvrhi::TextureDimension::Texture2DArray;

        const nvrhi::TextureDesc textureDesc{
            .width = m_Width,
            .height = m_Height,
            .arraySize = spec.IsCubemap ? 6u : spec.ArraySize,
            .mipLevels = spec.MipLevels,
            .format = spec.ImageFormat,
            .dimension = dimension,
            .debugName = spec.DebugName,
            .isRenderTarget = spec.IsRenderTarget,
            .initialState = spec.InitialState,
            .keepInitialState = spec.AutomaticStateTracking,
        };

        m_Texture = device->createTexture(textureDesc);
        m_MipLevels = m_Texture->getDesc().mipLevels;
        m_Stride = GetStride(m_Texture->getDesc().format);
        IsLoaded.store(true, std::memory_order_release);
    }

    Image::Image(const ImageSpecification& spec, void* existingImage)
        : m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
    {
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
        IsLoaded.store(true, std::memory_order_release);
    }

    auto Image::SetData(const void* data, uint64_t size, const nvrhi::CommandListHandle& cmdList) -> void
    {
        EP_PROFILE_FN("Image::SetData");
        EP_ASSERT(m_Stride > 0);

        const auto device = DeviceManager::Get()->GetDevice();
        const auto cmd = cmdList ? cmdList : device->createCommandList();

        if (!cmdList)
            cmd->open();

        cmd->writeTexture(m_Texture, 0, 0, data, m_Stride);

        if (!cmdList)
        {
            cmd->close();
            device->executeCommandList(cmd);
        }
    }

    auto Image::DecodeToRGBA8(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight) -> Buffer
    {
        EP_PROFILE_FN("Image::DecodeToRGBA8");

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

    auto Image::CalculateMipLevels(const uint32_t width, const uint32_t height) -> uint32_t
    {
        EP_ASSERT(width > 0 && height > 0);
        const uint32_t mipLevels =
            1 + static_cast<uint32_t>(glm::floor(glm::log2(glm::max(static_cast<float>(width), static_cast<float>(height)))));
        EP_ASSERT(mipLevels > 0);
        return mipLevels;
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

    auto Image::RegisterBindlessIndex(const nvrhi::TextureSubresourceSet& subresources) -> uint32_t
    {
        const auto resolved = subresources.resolve(m_Texture->getDesc(), false);
        if (m_BindlessHandles.contains(resolved))
            return m_BindlessHandles.at(resolved)->Index;

        const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
        m_BindlessHandles.emplace(resolved, CreateRef<BindlessHandle>(descriptorManager->Register(shared_from_this(), resolved)));

        return m_BindlessHandles.at(resolved)->Index;
    }

    auto Image::GetBindlessIndex(const nvrhi::TextureSubresourceSet& subresources) -> uint32_t
    {
        const auto resolved = subresources.resolve(m_Texture->getDesc(), false);
        if (m_BindlessHandles.contains(resolved))
            return m_BindlessHandles.at(resolved)->Index;

        Log::Warn("GetBindlessIndex called on a image that did not yet have a bindless index, registering now...");

        return RegisterBindlessIndex(subresources);
    }

    auto Image::GenerateFallbackImage() -> Ref<Image>
    {
        EP_PROFILE_FN("Image::GenerateFallbackImage");

        constexpr uint32_t imageSize = 16;
        ScopedBuffer buffer(imageSize * imageSize * 4);

        for (uint32_t y = 0; y < imageSize; y++)
        {
            for (uint32_t x = 0; x < imageSize; x++)
            {
                const bool magenta = ((x / 2) + (y / 2)) % 2 == 0;
                uint8_t* p = buffer.Data() + (y * imageSize + x) * 4;
                p[0] = magenta ? 255 : 0;
                p[1] = 0;
                p[2] = magenta ? 255 : 0;
                p[3] = 255;
            }
        }

        const ImageSpecification spec{
            .ImageFormat = nvrhi::Format::SRGBA8_UNORM,
            .Width = imageSize,
            .Height = imageSize,
            .DebugName = "Fallback Image",
        };

        auto image = Image::Create(spec);
        image->SetData(buffer.Data(), buffer.Size());
        image->RegisterBindlessIndex();

        return image;
    }

    auto Image::Create(const ImageSpecification& spec) -> Ref<Image>
    {
        EP_PROFILE_FN("Image::Create");
        return Ref<Image>(new Image(spec));
    }

    auto Image::Create(const ImageSpecification& spec, void* existingImage) -> Ref<Image>
    {
        EP_PROFILE_FN("Image::Create");
        return Ref<Image>(new Image(spec, existingImage));
    }

    auto Image::Create(const ImageSpecification& spec, const ImageSource& source, const nvrhi::CommandListHandle& cmdList) -> Ref<Image>
    {
        EP_PROFILE_FN("Image::Create");
        EP_ASSERT(!spec.IsCubemap);
        EP_ASSERT(spec.ArraySize == 1);

        auto image = Ref<Image>(new Image());
        image->m_Specification = spec;

        const auto* sourcePath = std::get_if<std::filesystem::path>(&source);
        const auto* sourceBuffer = std::get_if<Buffer>(&source);
        EP_ASSERT(sourcePath || sourceBuffer);

        ImageSource taskSource = sourcePath ? ImageSource(*sourcePath) : ImageSource(Buffer::Copy(*sourceBuffer));
        const auto& threadPool = Application::Get().GetThreadPool();
        auto result = CreateRef<TaskResult<ImageTaskResult>>();

        image->m_LoadTaskId = threadPool->QueueTask(
            [result, taskSource]()
            {
                ImageTaskResult taskResult;
                DecodeImageData(taskSource, taskResult);
                result->emplace(std::move(taskResult));
            },
            [image, result, taskSource, cmdList](const TaskStatus status) mutable
            {
                if (auto* buffer = std::get_if<Buffer>(&taskSource))
                    buffer->Release();

                if (status != TaskStatus::Completed || !result->has_value())
                    return;

                Renderer::Submit(
                    [image, result, cmdList]()
                    {
                        const auto& taskResult = result->value();
                        image->m_Width = taskResult.Width;
                        image->m_Height = taskResult.Height;
                        image->m_Specification.Width = taskResult.Width;
                        image->m_Specification.Height = taskResult.Height;
                        if (image->m_Specification.ImageFormat == nvrhi::Format::UNKNOWN)
                            image->m_Specification.ImageFormat = image->SelectFormat(taskResult.Channels, taskResult.IsHdr);

                        const auto device = DeviceManager::Get()->GetDevice();
                        const nvrhi::TextureDesc textureDesc{
                            .width = image->m_Specification.Width,
                            .height = image->m_Specification.Height,
                            .format = image->m_Specification.ImageFormat,
                            .debugName = image->m_Specification.DebugName,
                            .isRenderTarget = image->m_Specification.IsRenderTarget,
                            .initialState = image->m_Specification.InitialState,
                            .keepInitialState = image->m_Specification.AutomaticStateTracking,
                        };

                        image->m_Texture = device->createTexture(textureDesc);
                        image->m_MipLevels = image->m_Texture->getDesc().mipLevels;
                        image->m_Stride = image->GetStride(image->m_Texture->getDesc().format);

                        const auto commandList = cmdList ? cmdList
                                                         : device->createCommandList(
                                                               nvrhi::CommandListParameters{
                                                                   .enableImmediateExecution = false,
                                                               }
                                                           );
                        commandList->open();
                        commandList->beginTrackingTextureState(image->m_Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
                        commandList->writeTexture(image->m_Texture, 0, 0, taskResult.Pixels.Data(), image->m_Stride);
                        commandList->setPermanentTextureState(image->m_Texture, nvrhi::ResourceStates::ShaderResource);
                        commandList->commitBarriers();
                        commandList->close();

                        if (!cmdList)
                        {
                            device->executeCommandList(commandList);
                            image->IsLoaded.store(true, std::memory_order_release);
                        }
                    }
                );
            }
        );

        return image;
    }

    auto Image::DecodeImageData(const ImageSource& source, ImageTaskResult& result) -> void
    {
        EP_PROFILE_FN("Image::DecodeImageData");

        const auto* path = std::get_if<std::filesystem::path>(&source);
        const auto* buffer = std::get_if<Buffer>(&source);
        EP_ASSERT(path || buffer);

        int width = 0;
        int height = 0;
        int channels = 0;
        void* decodedData = nullptr;

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

        const uint64_t pixelSize = isHdr ? sizeof(float) : sizeof(uint8_t);
        Buffer pixels = Buffer::Copy(
            static_cast<uint8_t*>(decodedData),
            static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(channels) * pixelSize
        );
        stbi_image_free(decodedData);

        result.Pixels = ScopedBuffer(std::move(pixels));
        result.Width = static_cast<uint32_t>(width);
        result.Height = static_cast<uint32_t>(height);
        result.Channels = static_cast<uint32_t>(channels);
        result.IsHdr = isHdr;
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
