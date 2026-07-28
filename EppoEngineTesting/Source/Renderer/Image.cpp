#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TempDir.h"

#include "Renderer/Image.h"

using namespace Eppo;

SUITE(Renderer)
{
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
            REQUIRE CHECK(FS::WriteBytes(path, bytes, true));
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
            REQUIRE CHECK(device->waitForIdle());

            size_t rowPitch = 0;
            const auto* data = static_cast<const float*>(
                device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &rowPitch)
            );
            REQUIRE CHECK(data != nullptr);
            CHECK(rowPitch >= sizeof(float) * 4);

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

            REQUIRE CHECK(image->GetTexture() != nullptr);
            CHECK_EQUAL(1u, image->GetWidth());
            CHECK_EQUAL(1u, image->GetHeight());
            CHECK(image->GetFormat() == nvrhi::Format::RGBA32_FLOAT);

            const std::array<float, 4> pixels = ReadFloatPixels(image);
            CHECK_CLOSE(2.0f, pixels[0], 0.0001f);
            CHECK_CLOSE(1.0f, pixels[1], 0.0001f);
            CHECK_CLOSE(0.5f, pixels[2], 0.0001f);
            CHECK_CLOSE(1.0f, pixels[3], 0.0001f);
        }
    }

    TEST(Image_HdrFileUploadsFourUnclampedFloatChannels)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const Testing::TempDir tempDir;
        const std::vector<char> bytes = MakeHdrBytes();
        const std::filesystem::path path = WriteHdrImage(tempDir, bytes);

        CheckUploadedImage(path);
    }

    TEST(Image_HdrMemoryUploadsFourUnclampedFloatChannels)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        std::vector<char> bytes = MakeHdrBytes();
        const Buffer buffer(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());

        CheckUploadedImage(buffer);
    }
}
