#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TestContext.h"
#include "Support/TempDir.h"

#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Platform/Vulkan/VulkanGpuProfiler.h"
#include "Project/Project.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

#include <GLFW/glfw3.h>
#include <spdlog/sinks/base_sink.h>

#include <atomic>
#include <mutex>

using namespace Eppo;

namespace
{
    class ErrorCountingSink final : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        [[nodiscard]] auto ErrorCount() const -> uint32_t { return m_ErrorCount.load(); }

    protected:
        auto sink_it_(const spdlog::details::log_msg& msg) -> void override
        {
            if (msg.level >= spdlog::level::err)
                m_ErrorCount.fetch_add(1);
        }
        auto flush_() -> void override {}

    private:
        std::atomic<uint32_t> m_ErrorCount{ 0 };
    };

    struct Rgba8Readback
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;
    };

    class PrimitiveProjectFixture
    {
    public:
        PrimitiveProjectFixture()
            : m_Previous(Project::GetActive()), m_ProjectDirectory(m_Directory.File("Project")), m_AssetManager(CreateRef<AssetManager>())
        {
            std::filesystem::create_directories(m_ProjectDirectory / "Assets");
            Project::New(ProjectSpecification{ .Name = "PrimitiveRendering", .ProjectDirectory = m_ProjectDirectory }, m_AssetManager);
        }

        ~PrimitiveProjectFixture() { Project::SetActive(m_Previous); }

    private:
        Ref<Project> m_Previous;
        Testing::TempDir m_Directory;
        std::filesystem::path m_ProjectDirectory;
        Ref<AssetManager> m_AssetManager;
    };

    class ActiveProjectRestorer
    {
    public:
        ActiveProjectRestorer()
            : m_Previous(Project::GetActive())
        {}

        ~ActiveProjectRestorer() { Project::SetActive(m_Previous); }

    private:
        Ref<Project> m_Previous;
    };

    // Uniform equirectangular RADIANCE HDR: every RGBE pixel decodes to (2.0, 1.0, 0.5) linear.
    // 4x2 stays under stb's RLE threshold, so it is read as a flat scanline.
    [[nodiscard]] auto MakeUniformHdr(const uint32_t width, const uint32_t height) -> std::vector<char>
    {
        const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", height, width);
        std::vector<char> bytes(header.begin(), header.end());
        for (uint32_t i = 0; i < width * height; i++)
            bytes.insert(bytes.end(), { static_cast<char>(128), static_cast<char>(64), static_cast<char>(32), static_cast<char>(130) });
        return bytes;
    }

    [[nodiscard]] auto MakeLatitudeHdr(const uint32_t width, const uint32_t height) -> std::vector<char>
    {
        const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", height, width);
        std::vector<char> bytes(header.begin(), header.end());
        for (uint32_t y = 0; y < height; y++)
        {
            const bool top = y < height / 2u;
            for (uint32_t x = 0; x < width; x++)
            {
                if (top)
                    bytes.insert(bytes.end(), { static_cast<char>(128), 0, 0, static_cast<char>(129) });
                else
                    bytes.insert(bytes.end(), { 0, 0, static_cast<char>(128), static_cast<char>(129) });
            }
        }
        return bytes;
    }

    [[nodiscard]] auto MakeHotspotHdr(const uint32_t width, const uint32_t height) -> std::vector<char>
    {
        const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", height, width);
        std::vector<char> bytes(header.begin(), header.end());
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                if (x == width / 2u && y == height / 4u)
                    bytes.insert(
                        bytes.end(), { static_cast<char>(128), static_cast<char>(128), static_cast<char>(128), static_cast<char>(145) }
                    );
                else
                    bytes.insert(bytes.end(), { 0, 0, 0, 0 });
            }
        }
        return bytes;
    }

    [[nodiscard]] auto ReadRgba8(const Ref<Image>& image) -> Rgba8Readback
    {
        REQUIRE CHECK(image != nullptr);
        REQUIRE CHECK(image->GetFormat() == nvrhi::Format::RGBA8_UNORM);

        const auto device = Testing::AppHarness::Get()->GetDeviceManager()->GetDevice();
        const auto stagingTexture = device->createStagingTexture(image->GetTexture()->getDesc(), nvrhi::CpuAccessMode::Read);
        const auto commandList = device->createCommandList();

        commandList->open();
        commandList->copyTexture(stagingTexture, nvrhi::TextureSlice{}, image->GetTexture(), nvrhi::TextureSlice{});
        commandList->close();
        device->executeCommandList(commandList);
        REQUIRE CHECK(device->waitForIdle());

        size_t rowPitch = 0;
        const auto* mapped = static_cast<const uint8_t*>(
            device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &rowPitch)
        );
        REQUIRE CHECK(mapped != nullptr);

        Rgba8Readback readback{
            .Width = image->GetWidth(),
            .Height = image->GetHeight(),
            .Pixels = std::vector<uint8_t>(static_cast<size_t>(image->GetWidth()) * image->GetHeight() * 4u),
        };
        const size_t packedRowSize = static_cast<size_t>(readback.Width) * 4u;
        REQUIRE CHECK(rowPitch >= packedRowSize);
        for (uint32_t y = 0; y < readback.Height; y++)
            std::memcpy(
                readback.Pixels.data() + static_cast<size_t>(y) * packedRowSize, mapped + static_cast<size_t>(y) * rowPitch,
                packedRowSize
            );

        device->unmapStagingTexture(stagingTexture);
        return readback;
    }

    [[nodiscard]] auto ProjectToPixel(
        const EditorCamera& camera, const glm::vec3& worldPosition, const uint32_t width, const uint32_t height
    ) -> glm::ivec2
    {
        const glm::vec4 clip = camera.GetViewProjection() * glm::vec4(worldPosition, 1.0f);
        REQUIRE CHECK(clip.w > 0.0f);

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        REQUIRE CHECK(glm::abs(ndc.x) <= 1.0f);
        REQUIRE CHECK(glm::abs(ndc.y) <= 1.0f);

        const int32_t x = static_cast<int32_t>((ndc.x * 0.5f + 0.5f) * static_cast<float>(width - 1u));
        const int32_t y = static_cast<int32_t>((-ndc.y * 0.5f + 0.5f) * static_cast<float>(height - 1u));
        return { x, y };
    }

    [[nodiscard]] auto AverageLuminance(const Rgba8Readback& readback, const glm::ivec2 center, const int32_t radius = 2) -> float
    {
        float luminance = 0.0f;
        uint32_t sampleCount = 0;

        for (int32_t y = center.y - radius; y <= center.y + radius; y++)
        {
            for (int32_t x = center.x - radius; x <= center.x + radius; x++)
            {
                if (x < 0 || y < 0 || x >= static_cast<int32_t>(readback.Width) || y >= static_cast<int32_t>(readback.Height))
                    continue;

                const size_t index = (static_cast<size_t>(y) * readback.Width + static_cast<size_t>(x)) * 4u;
                const glm::vec3 color(
                    static_cast<float>(readback.Pixels[index]) / 255.0f, static_cast<float>(readback.Pixels[index + 1u]) / 255.0f,
                    static_cast<float>(readback.Pixels[index + 2u]) / 255.0f
                );
                luminance += glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
                sampleCount++;
            }
        }

        REQUIRE CHECK(sampleCount > 0);
        return luminance / static_cast<float>(sampleCount);
    }

    [[nodiscard]] auto ReadPixel(const Rgba8Readback& readback, const glm::ivec2 position) -> glm::vec3
    {
        REQUIRE CHECK(position.x >= 0 && position.y >= 0);
        REQUIRE CHECK(position.x < static_cast<int32_t>(readback.Width) && position.y < static_cast<int32_t>(readback.Height));

        const size_t index = (static_cast<size_t>(position.y) * readback.Width + static_cast<size_t>(position.x)) * 4u;
        return {
            static_cast<float>(readback.Pixels[index]) / 255.0f,
            static_cast<float>(readback.Pixels[index + 1u]) / 255.0f,
            static_cast<float>(readback.Pixels[index + 2u]) / 255.0f,
        };
    }

    [[nodiscard]] auto AverageNeighbourLuminanceDelta(
        const Rgba8Readback& readback, const glm::ivec2 center, const int32_t radius
    ) -> float
    {
        float delta = 0.0f;
        uint32_t sampleCount = 0;
        const glm::vec3 weights(0.2126f, 0.7152f, 0.0722f);

        for (int32_t y = center.y - radius; y < center.y + radius; y++)
        {
            for (int32_t x = center.x - radius; x < center.x + radius; x++)
            {
                if (glm::distance(glm::vec2(x, y), glm::vec2(center)) > static_cast<float>(radius))
                    continue;

                const float luminance = glm::dot(ReadPixel(readback, { x, y }), weights);
                delta += glm::abs(luminance - glm::dot(ReadPixel(readback, { x + 1, y }), weights));
                delta += glm::abs(luminance - glm::dot(ReadPixel(readback, { x, y + 1 }), weights));
                sampleCount += 2u;
            }
        }

        REQUIRE CHECK(sampleCount > 0);
        return delta / static_cast<float>(sampleCount);
    }

    auto ConfigureBloomScene(const Ref<Scene>& scene) -> void
    {
        auto& environment = scene->GetEnvironmentSettings();
        environment.ZenithColor = glm::vec3(0.0f);
        environment.HorizonColor = glm::vec3(0.0f);
        environment.GroundColor = glm::vec3(0.0f);
        environment.AmbientIntensity = 0.0f;

        const auto& assetManager = Project::GetActive()->GetAssetManager();
        const Ref<Mesh> mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cube));
        REQUIRE CHECK(mesh != nullptr);
        mesh->GetMaterial(0)->EmissiveFactor = glm::vec3(8.0f, 4.0f, 1.0f);

        Entity emitter = scene->CreateEntity("Bloom emitter");
        emitter.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);
        emitter.GetComponent<TransformComponent>().Scale = glm::vec3(0.25f);
    }
}

// End-to-end rendering over real frames: these drive a Scene through the SceneRenderer
// on the booted graphical harness, so they need a display + GPU.
SUITE(Renderer)
{
    TEST(SceneRenderer_BloomIntensityRaisesNeighbourLuminance)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        PrimitiveProjectFixture project;
        const Ref<Scene> scene = ctx.GetScene();
        ConfigureBloomScene(scene);

        auto& bloom = scene->GetBloomSettings();
        bloom.Threshold = 0.5f;
        bloom.Knee = 0.25f;
        bloom.Intensity = 0.0f;
        bloom.Radius = 1.0f;

        constexpr uint32_t width = 256u;
        constexpr uint32_t height = 256u;
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = width, .Height = height });
        EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
        camera.SetViewportSize(width, height);

        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });
        const Rgba8Readback disabled = ReadRgba8(sceneRenderer->GetFinalImage());

        bloom.Intensity = 0.55f;
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });
        const Rgba8Readback enabled = ReadRgba8(sceneRenderer->GetFinalImage());

        const glm::ivec2 neighbour = ProjectToPixel(camera, glm::vec3(0.5f, 0.0f, 0.0f), width, height);
        CHECK(AverageLuminance(enabled, neighbour) > AverageLuminance(disabled, neighbour) + 0.02f);
    }

    TEST(SceneRenderer_BloomRadiusWidensHalo)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        PrimitiveProjectFixture project;
        const Ref<Scene> scene = ctx.GetScene();
        ConfigureBloomScene(scene);

        auto& bloom = scene->GetBloomSettings();
        bloom.Threshold = 0.5f;
        bloom.Knee = 0.25f;
        bloom.Intensity = 0.55f;
        bloom.Radius = 0.0f;

        constexpr uint32_t width = 256u;
        constexpr uint32_t height = 256u;
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = width, .Height = height });
        EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
        camera.SetViewportSize(width, height);

        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });
        const Rgba8Readback narrow = ReadRgba8(sceneRenderer->GetFinalImage());

        bloom.Radius = 4.0f;
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });
        const Rgba8Readback wide = ReadRgba8(sceneRenderer->GetFinalImage());

        const glm::ivec2 farNeighbour = ProjectToPixel(camera, glm::vec3(1.1f, 0.0f, 0.0f), width, height);
        CHECK(AverageLuminance(wide, farNeighbour) > AverageLuminance(narrow, farNeighbour) + 0.04f);
    }

    TEST(SceneRenderer_PointLightAndGradientSky_RendersWithoutError)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const Ref<Scene> scene = ctx.GetScene();

        Entity lamp = scene->CreateEntity("Lamp");
        lamp.GetComponent<TransformComponent>().Translation = { 2.0f, 3.0f, 2.0f };
        auto& light = lamp.AddComponent<PointLightComponent>();
        light.Color = { 1.0f, 0.8f, 0.6f };
        light.Intensity = 15.0f;

        scene->GetEnvironmentSettings().AmbientIntensity = 0.75f;

        const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
        const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);

        // Render across several real frames to cycle the frames-in-flight indices.
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        CHECK(sceneRenderer->GetFinalImage() != nullptr);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(SceneRenderer_HdrSceneTonemapsBeforeDepthAwareWireframes)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const Ref<Scene> scene = ctx.GetScene();
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 64u, .Height = 64u });
        Entity overlay = scene->CreateEntity("Depth-aware wireframe");
        overlay.AddComponent<BoxColliderComponent>();
        sceneRenderer->SetDebugRenderingEnabled(true);
        sceneRenderer->SetShowColliders(true);

        const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        sceneRenderer->Resize(96u, 48u);
        ctx.AdvanceFrames(1, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Ref<Image>& finalImage = sceneRenderer->GetFinalImage();
        REQUIRE CHECK(finalImage != nullptr);
        CHECK(finalImage->GetFormat() == nvrhi::Format::RGBA8_UNORM);
        CHECK_EQUAL(96u, finalImage->GetWidth());
        CHECK_EQUAL(48u, finalImage->GetHeight());
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(SceneRenderer_DirectionalShadowDarkensReceiverAndSurvivesResize)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        PrimitiveProjectFixture project;
        const Ref<Scene> scene = ctx.GetScene();
        scene->GetEnvironmentSettings().ZenithColor = glm::vec3(0.0f);
        scene->GetEnvironmentSettings().HorizonColor = glm::vec3(0.0f);
        scene->GetEnvironmentSettings().GroundColor = glm::vec3(0.0f);
        scene->GetEnvironmentSettings().AmbientIntensity = 0.0f;

        Entity receiver = scene->CreateEntity("Shadow receiver");
        receiver.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);
        receiver.GetComponent<TransformComponent>().Translation = { 0.0f, -0.05f, 0.0f };
        receiver.GetComponent<TransformComponent>().Scale = { 3.0f, 0.05f, 3.0f };

        Entity caster = scene->CreateEntity("Shadow caster");
        caster.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);
        caster.GetComponent<TransformComponent>().Translation = { 0.0f, 0.5f, 0.0f };
        caster.GetComponent<TransformComponent>().Scale = glm::vec3(0.5f);

        Entity sun = scene->CreateEntity("Sun");
        auto& directionalLight = sun.AddComponent<DirectionalLightComponent>();
        directionalLight.Direction = glm::normalize(glm::vec3(0.6f, -1.0f, 0.3f));
        directionalLight.Intensity = 8.0f;

        constexpr uint32_t initialWidth = 256u;
        constexpr uint32_t initialHeight = 256u;
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = initialWidth, .Height = initialHeight });
        EditorCamera camera(glm::vec3(0.0f, 5.0f, 16.0f), -32.0f, -90.0f);
        camera.SetViewportSize(initialWidth, initialHeight);

        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Rgba8Readback readback = ReadRgba8(sceneRenderer->GetFinalImage());
        const float shadowed = AverageLuminance(readback, ProjectToPixel(camera, glm::vec3(0.3f, 0.001f, 0.15f), initialWidth, initialHeight));
        const float lit =
            AverageLuminance(readback, ProjectToPixel(camera, glm::vec3(-1.5f, 0.001f, 0.0f), initialWidth, initialHeight));
        CHECK(lit > shadowed + 0.05f);

        constexpr uint32_t resizedWidth = 320u;
        constexpr uint32_t resizedHeight = 180u;
        sceneRenderer->Resize(resizedWidth, resizedHeight);
        camera.SetViewportSize(resizedWidth, resizedHeight);
        ctx.AdvanceFrames(2, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Ref<Image>& finalImage = sceneRenderer->GetFinalImage();
        REQUIRE CHECK(finalImage != nullptr);
        CHECK_EQUAL(resizedWidth, finalImage->GetWidth());
        CHECK_EQUAL(resizedHeight, finalImage->GetHeight());
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(SceneRenderer_SkyboxEnvironment_BakesIblAndRendersCleanly)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        // A project holding a uniform equirectangular HDR the renderer can bake IBL from.
        const Ref<Project> previous = Project::GetActive();
        const Testing::TempDir dir;
        const auto projectDirectory = dir.File("Project");
        std::filesystem::create_directories(projectDirectory / "Assets" / "Textures");
        const Ref<AssetManager> assetManager = CreateRef<AssetManager>();
        Project::New(ProjectSpecification{ .Name = "Skybox", .ProjectDirectory = projectDirectory }, assetManager);

        const auto hdrPath = projectDirectory / "Assets" / "Textures" / "uniform.hdr";
        REQUIRE CHECK(FS::WriteBytes(hdrPath, MakeUniformHdr(4u, 2u), true));
        const Ref<Asset> asset = CreateRef<Asset>();
        asset->Handle = AssetHandle(800);
        REQUIRE CHECK(assetManager->CreateAsset(hdrPath, asset));

        const Ref<Scene> scene = ctx.GetScene();
        scene->GetEnvironmentSettings().SkyboxHandle = AssetHandle(800);
        scene->GetEnvironmentSettings().AmbientIntensity = 0.25f;
        scene->GetBloomSettings().Intensity = 0.0f;

        // A mesh so the geometry pass samples the baked irradiance/prefilter/LUT too.
        const Ref<Mesh> mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Sphere));
        REQUIRE CHECK(mesh != nullptr);
        mesh->GetMaterial(0)->BaseColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        mesh->GetMaterial(0)->Metallic = 0.0f;
        mesh->GetMaterial(0)->Roughness = 1.0f;

        Entity sphere = scene->CreateEntity("Sphere");
        sphere.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Sphere);

        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 128u, .Height = 128u });
        EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
        camera.SetViewportSize(128u, 128u);

        // The bake fires on the first frame's SubmitEnvironment; rendering several frames sends the
        // baked cubes/LUT through both the geometry and skybox passes under the validation layer.
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Ref<Image>& finalImage = sceneRenderer->GetFinalImage();
        REQUIRE CHECK(finalImage != nullptr);
        CHECK_EQUAL(128u, finalImage->GetWidth());
        CHECK(Testing::AppHarness::Get()->IsRunning());

        const glm::vec3 centerColor = ReadPixel(ReadRgba8(finalImage), glm::ivec2(64));
        CHECK(centerColor.r < 0.9f);
        CHECK(centerColor.r > centerColor.g + 0.05f);
        CHECK(centerColor.g > centerColor.b + 0.08f);

        Project::SetActive(previous);
    }

    TEST(SceneRenderer_EquirectangularSkyMapsTopToPositiveY)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const ActiveProjectRestorer restoreProject;
        const Testing::TempDir dir;
        const auto projectDirectory = dir.File("Project");
        std::filesystem::create_directories(projectDirectory / "Assets" / "Textures");
        const Ref<AssetManager> assetManager = CreateRef<AssetManager>();
        Project::New(ProjectSpecification{ .Name = "SkyOrientation", .ProjectDirectory = projectDirectory }, assetManager);

        const auto hdrPath = projectDirectory / "Assets" / "Textures" / "latitude.hdr";
        REQUIRE CHECK(FS::WriteBytes(hdrPath, MakeLatitudeHdr(8u, 4u), true));
        const Ref<Asset> asset = CreateRef<Asset>();
        asset->Handle = AssetHandle(801);
        REQUIRE CHECK(assetManager->CreateAsset(hdrPath, asset));

        const Ref<Scene> scene = ctx.GetScene();
        scene->GetEnvironmentSettings().SkyboxHandle = AssetHandle(801);
        scene->GetBloomSettings().Intensity = 0.0f;

        constexpr uint32_t size = 64u;
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = size, .Height = size });
        EditorCamera upward(glm::vec3(0.0f), 89.0f, 0.0f);
        upward.SetViewportSize(size, size);
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, upward); });
        const glm::vec3 upColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

        EditorCamera downward(glm::vec3(0.0f), -89.0f, 0.0f);
        downward.SetViewportSize(size, size);
        ctx.AdvanceFrames(2, [&](float) { scene->OnRenderEditor(sceneRenderer, downward); });
        const glm::vec3 downColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

        EditorCamera upperSide(glm::vec3(0.0f), 30.0f, 0.0f);
        upperSide.SetViewportSize(size, size);
        ctx.AdvanceFrames(2, [&](float) { scene->OnRenderEditor(sceneRenderer, upperSide); });
        const glm::vec3 upperSideColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

        EditorCamera lowerSide(glm::vec3(0.0f), -30.0f, 0.0f);
        lowerSide.SetViewportSize(size, size);
        ctx.AdvanceFrames(2, [&](float) { scene->OnRenderEditor(sceneRenderer, lowerSide); });
        const glm::vec3 lowerSideColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

        CHECK(upColor.r > upColor.b + 0.25f);
        CHECK(downColor.b > downColor.r + 0.25f);
        CHECK(upperSideColor.r > upperSideColor.b + 0.25f);
        CHECK(lowerSideColor.b > lowerSideColor.r + 0.25f);
    }

    TEST(SceneRenderer_RoughIblSuppressesHighFrequencyFireflies)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const ActiveProjectRestorer restoreProject;
        const Testing::TempDir dir;
        const auto projectDirectory = dir.File("Project");
        std::filesystem::create_directories(projectDirectory / "Assets" / "Textures");
        const Ref<AssetManager> assetManager = CreateRef<AssetManager>();
        Project::New(ProjectSpecification{ .Name = "IblFiltering", .ProjectDirectory = projectDirectory }, assetManager);

        const auto hdrPath = projectDirectory / "Assets" / "Textures" / "hotspot.hdr";
        REQUIRE CHECK(FS::WriteBytes(hdrPath, MakeHotspotHdr(512u, 256u), true));
        const Ref<Asset> asset = CreateRef<Asset>();
        asset->Handle = AssetHandle(802);
        REQUIRE CHECK(assetManager->CreateAsset(hdrPath, asset));

        const Ref<Scene> scene = ctx.GetScene();
        scene->GetEnvironmentSettings().SkyboxHandle = AssetHandle(802);
        scene->GetBloomSettings().Intensity = 0.0f;

        const Ref<Mesh> mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Sphere));
        REQUIRE CHECK(mesh != nullptr);
        mesh->GetMaterial(0)->BaseColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        mesh->GetMaterial(0)->Metallic = 0.0f;
        mesh->GetMaterial(0)->Roughness = 1.0f;

        Entity sphere = scene->CreateEntity("Sphere");
        sphere.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Sphere);

        constexpr uint32_t size = 128u;
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = size, .Height = size });
        EditorCamera camera(glm::vec3(4.0f, 0.0f, 0.0f), 0.0f, 180.0f);
        camera.SetViewportSize(size, size);
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Rgba8Readback readback = ReadRgba8(sceneRenderer->GetFinalImage());
        const float neighbourDelta = AverageNeighbourLuminanceDelta(readback, glm::ivec2(size / 2u), 20);
        CHECK_CLOSE(0.0f, neighbourDelta, 0.005f);

        mesh->GetMaterial(0)->Metallic = 1.0f;
        mesh->GetMaterial(0)->Roughness = 0.5f;
        ctx.AdvanceFrames(2, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Rgba8Readback specularReadback = ReadRgba8(sceneRenderer->GetFinalImage());
        const float specularDelta = AverageNeighbourLuminanceDelta(specularReadback, glm::ivec2(size / 2u), 20);
        CHECK_CLOSE(0.0f, specularDelta, 0.05f);
    }

    // Regression: the Tracy GPU context must be set up on its own command buffer, not by wrapping an
    // nvrhi command list, which double-began/re-submitted the buffer and tripped Vulkan validation.
    TEST(VulkanGpuProfiler_Construction_EmitsNoVulkanValidationErrors)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const auto sink = std::make_shared<ErrorCountingSink>();
        Log::AddSink(sink);

        {
            const VulkanGpuProfiler profiler;
#if defined(TRACY_ENABLE)
            CHECK(profiler.GetNativeContext() != nullptr);
#endif
        }

        CHECK_EQUAL(0u, sink->ErrorCount());
    }

    TEST(Renderer_CompositeToSwapchain_SurvivesImageCyclingAndResize)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        Application* app = Testing::AppHarness::Get();
        const Ref<Scene> scene = ctx.GetScene();
        const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
        const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);
        const uint32_t imageCount = app->GetDeviceManager()->GetBackBufferCount();
        uint32_t renderedFrames = 0;

        app->GetImGuiLayer()->SetClearMainSwapchainTarget(false);
        ctx.AdvanceFrames(imageCount + 2, [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
            app->GetDeviceManager()->GetRenderer()->CompositeToSwapchain(sceneRenderer->GetFinalImage());
            renderedFrames++;
        });

        glfwSetWindowSize(app->GetWindow()->GetNative(), 960, 540);
        ctx.AdvanceFrames(imageCount + 2, [&](float)
        {
            const auto [width, height] = app->GetWindow()->GetFramebufferSize();
            if (width > 0 && height > 0)
                sceneRenderer->Resize(width, height);
            scene->OnRenderEditor(sceneRenderer, camera);
            app->GetDeviceManager()->GetRenderer()->CompositeToSwapchain(sceneRenderer->GetFinalImage());
            renderedFrames++;
        });

        CHECK_EQUAL((imageCount + 2) * 2, renderedFrames);
        CHECK(app->IsRunning());
    }
}
