#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TempDir.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#include <chrono>
#include <thread>

using namespace Eppo;

namespace
{
    auto WaitForImage(const Ref<Image>& image) -> bool
    {
        for (uint32_t frame = 0; frame < 120 && !image->IsLoaded.load(std::memory_order_acquire); frame++)
        {
            Testing::AppHarness::AdvanceFrames(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return image->IsLoaded.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto MakeHdrBytes() -> std::vector<char>
    {
        const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";
        std::vector<char> bytes(header.begin(), header.end());
        bytes.insert(bytes.end(), { static_cast<char>(128), static_cast<char>(64), static_cast<char>(32), static_cast<char>(130) });
        return bytes;
    }

    [[nodiscard]] auto MakeTgaBytes(const uint16_t width, const uint16_t height, const std::vector<uint8_t>& rgba) -> std::vector<char>
    {
        EP_REQUIRE(rgba.size() == static_cast<size_t>(width) * height * 4);

        std::vector<char> bytes(18 + rgba.size());
        bytes[2] = 2;
        bytes[12] = static_cast<char>(width & 0xff);
        bytes[13] = static_cast<char>(width >> 8);
        bytes[14] = static_cast<char>(height & 0xff);
        bytes[15] = static_cast<char>(height >> 8);
        bytes[16] = 32;
        bytes[17] = 0x28;

        for (size_t i = 0; i < rgba.size(); i += 4)
        {
            bytes[18 + i] = static_cast<char>(rgba[i + 2]);
            bytes[18 + i + 1] = static_cast<char>(rgba[i + 1]);
            bytes[18 + i + 2] = static_cast<char>(rgba[i]);
            bytes[18 + i + 3] = static_cast<char>(rgba[i + 3]);
        }

        return bytes;
    }

    auto WriteHdrImage(const Testing::TempDir& tempDir, const std::vector<char>& bytes) -> std::filesystem::path
    {
        const std::filesystem::path path = tempDir.File("Unclamped.hdr");
        EP_REQUIRE(FS::WriteBytes(path, bytes, true));
        return path;
    }

    [[nodiscard]] auto ReadFloatPixels(const Ref<Image>& image) -> std::array<float, 4>
    {
        const auto device = Testing::AppHarness::Get()->GetDeviceManager()->GetDevice();
        const auto stagingTexture = device->createStagingTexture(image->GetTexture()->getDesc(), nvrhi::CpuAccessMode::Read);
        const auto commandList = device->createCommandList();

        commandList->open();
        commandList->copyTexture(stagingTexture, nvrhi::TextureSlice{}, image->GetTexture(), nvrhi::TextureSlice{});
        commandList->close();
        device->executeCommandList(commandList);
        EP_REQUIRE(device->waitForIdle());

        size_t rowPitch = 0;
        const auto* data = static_cast<const float*>(
            device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &rowPitch)
        );
        EP_REQUIRE(data != nullptr);
        EXPECT_TRUE(rowPitch >= sizeof(float) * 4);

        const std::array pixels{ data[0], data[1], data[2], data[3] };
        device->unmapStagingTexture(stagingTexture);
        return pixels;
    }

    [[nodiscard]] auto ReadRgba8Pixels(const Ref<Image>& image, const uint32_t mipLevel) -> std::vector<uint8_t>
    {
        const auto device = Testing::AppHarness::Get()->GetDeviceManager()->GetDevice();
        const auto stagingTexture = device->createStagingTexture(image->GetTexture()->getDesc(), nvrhi::CpuAccessMode::Read);
        const auto commandList = device->createCommandList();
        const nvrhi::TextureSlice slice = nvrhi::TextureSlice{}.setMipLevel(mipLevel);

        commandList->open();
        commandList->copyTexture(stagingTexture, slice, image->GetTexture(), slice);
        commandList->close();
        device->executeCommandList(commandList);
        EP_REQUIRE(device->waitForIdle());

        size_t rowPitch = 0;
        const auto* data = static_cast<const uint8_t*>(
            device->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Read, &rowPitch)
        );
        EP_REQUIRE(data != nullptr);

        const uint32_t width = image->GetMipWidth(mipLevel);
        const uint32_t height = image->GetMipHeight(mipLevel);
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
        for (uint32_t y = 0; y < height; y++)
            std::memcpy(pixels.data() + static_cast<size_t>(y) * width * 4, data + y * rowPitch, static_cast<size_t>(width) * 4);

        device->unmapStagingTexture(stagingTexture);
        return pixels;
    }

    [[nodiscard]] auto CreateMipmappedImage(
        const uint16_t width, const uint16_t height, const std::vector<uint8_t>& rgba, const MipGenerationMode mipMode
    ) -> Ref<Image>
    {
        std::vector<char> bytes = MakeTgaBytes(width, height, rgba);
        const Buffer buffer(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
        return Image::Create(
            ImageSpecification{
                .ImageFormat = mipMode == MipGenerationMode::ColorSRGB ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM,
                .MipMode = mipMode,
                .DebugName = "Image mip generation test",
            },
            buffer
        );
    }

    auto CheckUploadedImage(const ImageSource& source) -> void
    {
        const Ref<Image> image = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA32_FLOAT,
                .DebugName = "Image HDR upload test",
            },
            source
        );

        EXPECT_FALSE(image->IsLoaded.load(std::memory_order_acquire));
        EP_REQUIRE(WaitForImage(image));
        EP_REQUIRE(image->GetTexture() != nullptr);
        EXPECT_EQ(1u, image->GetWidth());
        EXPECT_EQ(1u, image->GetHeight());
        EXPECT_TRUE(image->GetFormat() == nvrhi::Format::RGBA32_FLOAT);

        const std::array<float, 4> pixels = ReadFloatPixels(image);
        EXPECT_NEAR(2.0f, pixels[0], 0.0001f);
        EXPECT_NEAR(1.0f, pixels[1], 0.0001f);
        EXPECT_NEAR(0.5f, pixels[2], 0.0001f);
        EXPECT_NEAR(1.0f, pixels[3], 0.0001f);
    }
}

TEST(Renderer, Image_HdrFileUploadsFourUnclampedFloatChannels)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Testing::TempDir tempDir;
    const std::vector<char> bytes = MakeHdrBytes();
    const std::filesystem::path path = WriteHdrImage(tempDir, bytes);

    CheckUploadedImage(path);
}

TEST(Renderer, Image_HdrMemoryUploadsFourUnclampedFloatChannels)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    std::vector<char> bytes = MakeHdrBytes();
    const Buffer buffer(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());

    CheckUploadedImage(buffer);
}

TEST(Renderer, Image_MemorySourceIsOwnedUntilWorkerDecodesIt)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    Ref<Image> image;
    {
        std::vector<char> bytes = MakeHdrBytes();
        const Buffer buffer(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
        image = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA32_FLOAT,
                .DebugName = "Image owned memory test",
            },
            buffer
        );
    }

    EP_REQUIRE(WaitForImage(image));
    const std::array<float, 4> pixels = ReadFloatPixels(image);
    EXPECT_NEAR(2.0f, pixels[0], 0.0001f);
    EXPECT_NEAR(1.0f, pixels[1], 0.0001f);
    EXPECT_NEAR(0.5f, pixels[2], 0.0001f);
    EXPECT_NEAR(1.0f, pixels[3], 0.0001f);
}

TEST(Renderer, Image_LinearMipChainUploadsEveryLevel)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<uint8_t> rgba(4 * 2 * 4, 64);
    const Ref<Image> image = CreateMipmappedImage(4, 2, rgba, MipGenerationMode::Linear);

    EP_REQUIRE(WaitForImage(image));
    EXPECT_EQ(3u, image->GetMipLevels());
    EXPECT_EQ(1u, image->GetMipWidth(2));
    EXPECT_EQ(1u, image->GetMipHeight(2));

    const std::vector<uint8_t> pixels = ReadRgba8Pixels(image, 2);
    EXPECT_NEAR(64, pixels[0], 1);
    EXPECT_NEAR(64, pixels[1], 1);
    EXPECT_NEAR(64, pixels[2], 1);
}

TEST(Renderer, Image_SrgbMipFilteringUsesLinearLight)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<uint8_t> rgba{
        0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255,
    };
    const Ref<Image> image = CreateMipmappedImage(2, 2, rgba, MipGenerationMode::ColorSRGB);

    EP_REQUIRE(WaitForImage(image));
    const std::vector<uint8_t> pixels = ReadRgba8Pixels(image, 1);
    EXPECT_NEAR(188, pixels[0], 3);
    EXPECT_NEAR(188, pixels[1], 3);
    EXPECT_NEAR(188, pixels[2], 3);
}

TEST(Renderer, Image_NormalMapMipsRemainNormalized)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<uint8_t> rgba{
        255, 128, 128, 255, 128, 255, 128, 255, 255, 128, 128, 255, 128, 255, 128, 255,
    };
    const Ref<Image> image = CreateMipmappedImage(2, 2, rgba, MipGenerationMode::NormalMap);

    EP_REQUIRE(WaitForImage(image));
    const std::vector<uint8_t> pixels = ReadRgba8Pixels(image, 1);
    const glm::vec3 normal = glm::vec3(pixels[0], pixels[1], pixels[2]) / 127.5f - 1.0f;
    EXPECT_NEAR(1.0f, glm::length(normal), 0.02f);
    EXPECT_GT(normal.x, 0.65f);
    EXPECT_GT(normal.y, 0.65f);
}

TEST(Renderer, Image_CubemapRenderTargetCreatesSixSlicesAndRequestedMips)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Image> image = Image::Create(ImageSpecification{
        .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
        .Width = 128u,
        .Height = 128u,
        .MipLevels = 5u,
        .IsCubemap = true,
        .IsRenderTarget = true,
        .DebugName = "Image cubemap test",
    });

    EP_REQUIRE(image->GetTexture() != nullptr);
    const nvrhi::TextureDesc& desc = image->GetTexture()->getDesc();
    EXPECT_TRUE(desc.dimension == nvrhi::TextureDimension::TextureCube);
    EXPECT_EQ(6u, desc.arraySize);
    EXPECT_EQ(5u, desc.mipLevels);
    EXPECT_EQ(128u, desc.width);
    EXPECT_EQ(128u, desc.height);
}

TEST(Renderer, Image_FallbackImageUploadsAndRegistersBindlessIndex)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Image> image = Image::GenerateFallbackImage();

    EP_REQUIRE(image != nullptr);
    EP_REQUIRE(image->GetTexture() != nullptr);
    EXPECT_EQ(16u, image->GetWidth());
    EXPECT_EQ(16u, image->GetHeight());
    EXPECT_TRUE(image->GetFormat() == nvrhi::Format::SRGBA8_UNORM);
    EXPECT_NE(std::numeric_limits<uint32_t>::max(), image->GetBindlessIndex());
}

TEST(Renderer, DescriptorManager_CubemapRegistersAsResource)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Image> image = Image::Create(ImageSpecification{
        .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
        .Width = 16u,
        .Height = 16u,
        .IsCubemap = true,
        .IsRenderTarget = true,
        .DebugName = "Bindless cubemap test",
    });
    const Ref<DescriptorManager> descriptorManager =
        Testing::AppHarness::Get()->GetDeviceManager()->GetRenderer()->GetDescriptorManager();

    const BindlessHandle handle = descriptorManager->Register(image);

    EXPECT_TRUE(handle.HeapType == BindlessHeapType::Resource);
    EXPECT_TRUE(handle.Index != std::numeric_limits<uint32_t>::max());
    EXPECT_TRUE(handle.Index < descriptorManager->GetResourceHeap()->Capacity);
    EXPECT_TRUE(image->GetTexture()->getDesc().dimension == nvrhi::TextureDimension::TextureCube);
}
