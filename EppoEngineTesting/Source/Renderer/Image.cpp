#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TempDir.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

using namespace Eppo;

namespace
{
    [[nodiscard]] auto MakeHdrBytes() -> std::vector<char>
    {
        const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";
        std::vector<char> bytes(header.begin(), header.end());
        bytes.insert(bytes.end(), { static_cast<char>(128), static_cast<char>(64), static_cast<char>(32), static_cast<char>(130) });
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

    auto CheckUploadedImage(const ImageSource& source) -> void
    {
        const Ref<Image> image = CreateRef<Image>(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA32_FLOAT,
                .DebugName = "Image HDR upload test",
            },
            source
        );

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

TEST(Renderer, Image_CubemapRenderTargetCreatesSixSlicesAndRequestedMips)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Image> image = CreateRef<Image>(ImageSpecification{
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

TEST(Renderer, DescriptorManager_CubemapRegistersAsResource)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Ref<Image> image = CreateRef<Image>(ImageSpecification{
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
