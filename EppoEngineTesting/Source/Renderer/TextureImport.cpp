#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TempDir.h"

#include "Asset/AssetManager.h"
#include "Project/Project.h"
#include "Renderer/Image.h"

using namespace Eppo;

// Registering a texture on disk and resolving it through GetOrLoadAsset<Image> is the
// guard that the whole IBL chain has a handle to bake from. Image creation needs a GPU,
// so this rides the graphical Renderer suite.
SUITE(Renderer)
{
    namespace
    {
        // Minimal uncompressed 2x2 24-bit TGA (stb decodes it to RGBA -> SRGBA8_UNORM).
        [[nodiscard]] auto MakeTgaBytes() -> std::vector<char>
        {
            std::vector<char> bytes(18, 0);
            bytes[2] = 2;  // uncompressed true-color
            bytes[12] = 2; // width lo
            bytes[14] = 2; // height lo
            bytes[16] = 24; // bits per pixel
            for (int i = 0; i < 4; i++)
                bytes.insert(bytes.end(), { static_cast<char>(64), static_cast<char>(128), static_cast<char>(200) });
            return bytes;
        }

        [[nodiscard]] auto MakeGrayscaleTgaBytes() -> std::vector<char>
        {
            std::vector<char> bytes(18, 0);
            bytes[2] = 3;  // uncompressed grayscale
            bytes[12] = 2; // width lo
            bytes[14] = 2; // height lo
            bytes[16] = 8; // bits per pixel
            bytes.insert(bytes.end(), { static_cast<char>(32), static_cast<char>(96), static_cast<char>(160), static_cast<char>(224) });
            return bytes;
        }

        // 1x1 RADIANCE HDR (matches the Image suite's fixture) -> RGBA32_FLOAT.
        [[nodiscard]] auto MakeHdrBytes() -> std::vector<char>
        {
            const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";
            std::vector<char> bytes(header.begin(), header.end());
            bytes.insert(bytes.end(), { static_cast<char>(128), static_cast<char>(64), static_cast<char>(32), static_cast<char>(130) });
            return bytes;
        }

        class TextureProjectFixture
        {
        public:
            TextureProjectFixture()
                : m_Previous(Project::GetActive()), m_ProjectDirectory(m_Directory.File("Project")),
                  m_AssetManager(CreateRef<AssetManager>())
            {
                std::filesystem::create_directories(m_ProjectDirectory / "Assets" / "Textures");
                Project::New(ProjectSpecification{ .Name = "TextureImport", .ProjectDirectory = m_ProjectDirectory }, m_AssetManager);
            }

            ~TextureProjectFixture() { Project::SetActive(m_Previous); }

            auto Register(const uint64_t handle, const std::string& filename, const std::vector<char>& bytes) -> AssetHandle
            {
                const auto path = m_ProjectDirectory / "Assets" / "Textures" / filename;
                REQUIRE CHECK(FS::WriteBytes(path, bytes, true));
                const Ref<Asset> asset = CreateRef<Asset>();
                asset->Handle = AssetHandle(handle);
                REQUIRE CHECK(m_AssetManager->CreateAsset(path, asset));
                return AssetHandle(handle);
            }

            [[nodiscard]] auto Manager() const -> const Ref<AssetManager>& { return m_AssetManager; }

        private:
            Ref<Project> m_Previous;
            Testing::TempDir m_Directory;
            std::filesystem::path m_ProjectDirectory;
            Ref<AssetManager> m_AssetManager;
        };
    }

    TEST(ImportTexture_LdrFile_LoadsAsSrgbaImageWithExpectedDimensions)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        TextureProjectFixture fixture;
        const AssetHandle handle = fixture.Register(700, "ldr.tga", MakeTgaBytes());

        const Ref<Image> image = fixture.Manager()->GetOrLoadAsset<Image>(handle);

        REQUIRE CHECK(image != nullptr);
        CHECK(image->GetTexture() != nullptr);
        CHECK_EQUAL(static_cast<uint64_t>(handle), static_cast<uint64_t>(image->Handle));
        CHECK_EQUAL(2u, image->GetWidth());
        CHECK_EQUAL(2u, image->GetHeight());
        CHECK(image->GetFormat() == nvrhi::Format::SRGBA8_UNORM);
    }

    TEST(ImportTexture_GrayscaleLdrFile_ExpandsToSrgbaImage)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        TextureProjectFixture fixture;
        const AssetHandle handle = fixture.Register(702, "grayscale.tga", MakeGrayscaleTgaBytes());

        const Ref<Image> image = fixture.Manager()->GetOrLoadAsset<Image>(handle);

        REQUIRE CHECK(image != nullptr);
        CHECK_EQUAL(2u, image->GetWidth());
        CHECK_EQUAL(2u, image->GetHeight());
        CHECK(image->GetFormat() == nvrhi::Format::SRGBA8_UNORM);
    }

    TEST(ImportTexture_HdrFile_LoadsAsFloatImage)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        TextureProjectFixture fixture;
        const AssetHandle handle = fixture.Register(701, "sky.hdr", MakeHdrBytes());

        const Ref<Image> image = fixture.Manager()->GetOrLoadAsset<Image>(handle);

        REQUIRE CHECK(image != nullptr);
        CHECK_EQUAL(1u, image->GetWidth());
        CHECK_EQUAL(1u, image->GetHeight());
        CHECK(image->GetFormat() == nvrhi::Format::RGBA32_FLOAT);
    }
}
