#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TestContext.h"
#include "TestSupport/TempDir.h"

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
            : m_Previous(Project::GetActive()), m_ProjectDirectory(m_Directory.File("Project")), m_AssetManager(Ref<AssetManager>::Create())
        {
            std::filesystem::create_directories(m_ProjectDirectory / "Assets");
            Project::New(ProjectSpecification{ .Name = "PrimitiveRendering", .ProjectDirectory = m_ProjectDirectory }, m_AssetManager);
        }

        ~PrimitiveProjectFixture() { Project::SetActive(m_Previous); }

        auto RegisterMesh(const uint64_t handle, const std::string& filename, const std::string& source) -> AssetHandle
        {
            const std::filesystem::path path = m_ProjectDirectory / "Assets" / filename;
            EP_REQUIRE(FS::WriteText(path, source, true));

            Ref<Asset> asset = Ref<Asset>::Create();
            asset->Handle = AssetHandle(handle);
            EP_REQUIRE(m_AssetManager->CreateAsset(path, asset));
            return asset->Handle;
        }

        [[nodiscard]] auto Manager() const -> Ref<AssetManager> { return m_AssetManager; }

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

    [[nodiscard]] auto MakeDoubleSidedTriangleGltf() -> std::string
    {
        return R"({
            "asset": { "version": "2.0" },
            "scene": 0,
            "scenes": [{ "nodes": [0] }],
            "nodes": [{ "mesh": 0 }],
            "meshes": [{
                "name": "Double-sided triangle",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 },
                    "indices": 3,
                    "material": 0
                }]
            }],
            "materials": [{
                "doubleSided": true,
                "emissiveFactor": [1.0, 0.25, 0.05],
                "extensions": { "KHR_materials_emissive_strength": { "emissiveStrength": 4.0 } }
            }],
            "extensionsUsed": ["KHR_materials_emissive_strength"],
            "buffers": [{
                "byteLength": 102,
                "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"
            }],
            "bufferViews": [
                { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
                { "buffer": 0, "byteOffset": 96, "byteLength": 6 }
            ],
            "accessors": [
                { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
                { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
                { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
                { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
            ]
        })";
    }

    [[nodiscard]] auto ReadRgba8(Ref<Image> image) -> Rgba8Readback
    {
        EP_REQUIRE(image != nullptr);
        EP_REQUIRE(image->GetFormat() == nvrhi::Format::RGBA8_UNORM);

        const auto device = Testing::AppHarness::Get()->GetDeviceManager()->GetDevice();
        const auto stagingTexture = device->createStagingTexture(image->GetTexture()->getDesc(), nvrhi::CpuAccessMode::Read);
        const auto commandList = device->createCommandList();

        commandList->open();
        commandList->copyTexture(stagingTexture, nvrhi::TextureSlice{}, image->GetTexture(), nvrhi::TextureSlice{});
        commandList->close();
        device->executeCommandList(commandList);
        EP_REQUIRE(device->waitForIdle());

        size_t rowPitch = 0;
        const auto* mapped = static_cast<const uint8_t*>(
            device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &rowPitch)
        );
        EP_REQUIRE(mapped != nullptr);

        Rgba8Readback readback{
            .Width = image->GetWidth(),
            .Height = image->GetHeight(),
            .Pixels = std::vector<uint8_t>(static_cast<size_t>(image->GetWidth()) * image->GetHeight() * 4u),
        };
        const size_t packedRowSize = static_cast<size_t>(readback.Width) * 4u;
        EP_REQUIRE(rowPitch >= packedRowSize);
        for (uint32_t y = 0; y < readback.Height; y++)
            std::memcpy(
                readback.Pixels.data() + static_cast<size_t>(y) * packedRowSize, mapped + static_cast<size_t>(y) * rowPitch, packedRowSize
            );

        device->unmapStagingTexture(stagingTexture);
        return readback;
    }

    [[nodiscard]] auto
    ProjectToPixel(const EditorCamera& camera, const glm::vec3& worldPosition, const uint32_t width, const uint32_t height) -> glm::ivec2
    {
        const glm::vec4 clip = camera.GetViewProjection() * glm::vec4(worldPosition, 1.0f);
        EP_REQUIRE(clip.w > 0.0f);

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        EP_REQUIRE(glm::abs(ndc.x) <= 1.0f);
        EP_REQUIRE(glm::abs(ndc.y) <= 1.0f);

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

        EP_REQUIRE(sampleCount > 0);
        return luminance / static_cast<float>(sampleCount);
    }

    [[nodiscard]] auto MaxLuminance(const Rgba8Readback& readback, const glm::ivec2 center, const int32_t radius = 4) -> float
    {
        float maxLuminance = 0.0f;

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
                maxLuminance = std::max(maxLuminance, glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f)));
            }
        }

        return maxLuminance;
    }

    [[nodiscard]] auto ReadPixel(const Rgba8Readback& readback, const glm::ivec2 position) -> glm::vec3
    {
        EP_REQUIRE(position.x >= 0 && position.y >= 0);
        EP_REQUIRE(position.x < static_cast<int32_t>(readback.Width) && position.y < static_cast<int32_t>(readback.Height));

        const size_t index = (static_cast<size_t>(position.y) * readback.Width + static_cast<size_t>(position.x)) * 4u;
        return {
            static_cast<float>(readback.Pixels[index]) / 255.0f,
            static_cast<float>(readback.Pixels[index + 1u]) / 255.0f,
            static_cast<float>(readback.Pixels[index + 2u]) / 255.0f,
        };
    }
}

// End-to-end rendering over real frames: these drive a Scene through the SceneRenderer
// on the booted graphical harness, so they need a display + GPU.

TEST(Renderer, SceneRenderer_HdrSceneDisplayConvertsBeforeDepthAwareWireframes)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    Ref<Scene> scene = ctx.GetScene();
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = 64u, .Height = 64u });
    Entity overlay = scene->CreateEntity("Depth-aware wireframe");
    overlay.AddComponent<BoxColliderComponent>();
    sceneRenderer->SetDebugRenderingEnabled(true);
    sceneRenderer->SetShowColliders(true);

    const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);
    ctx.AdvanceFrames(
        3,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );

    sceneRenderer->Resize(96u, 48u);
    ctx.AdvanceFrames(
        1,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );

    Ref<Image> finalImage = sceneRenderer->GetFinalImage();
    EP_REQUIRE(finalImage != nullptr);
    EXPECT_TRUE(finalImage->GetFormat() == nvrhi::Format::RGBA8_UNORM);
    EXPECT_EQ(96u, finalImage->GetWidth());
    EXPECT_EQ(48u, finalImage->GetHeight());
    EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
}

TEST(Renderer, SceneRenderer_DoubleSidedMaterialRendersFromBothSides)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    const AssetHandle triangleHandle = project.RegisterMesh(700u, "DoubleSidedTriangle.gltf", MakeDoubleSidedTriangleGltf());
    Ref<Mesh> triangleMesh = project.Manager()->GetOrLoadAsset(triangleHandle).As<Mesh>();
    EP_REQUIRE(triangleMesh != nullptr);
    Ref<Material> material = triangleMesh->GetMaterial(0);
    EP_REQUIRE(material != nullptr);

    Ref<Scene> scene = ctx.GetScene();
    auto& environment = scene->GetEnvironmentSettings();
    environment.ZenithColor = glm::vec3(0.0f);
    environment.HorizonColor = glm::vec3(0.0f);
    environment.GroundColor = glm::vec3(0.0f);
    environment.AmbientIntensity = 0.0f;
    scene->GetSsaoSettings().Intensity = 0.0f;
    scene->GetBloomSettings().Intensity = 0.0f;

    Entity triangle = scene->CreateEntity("Double-sided triangle");
    triangle.AddComponent<MeshComponent>().MeshHandle = triangleHandle;
    triangle.GetComponent<TransformComponent>().Translation = { -1.0f, -1.0f, 0.0f };
    triangle.GetComponent<TransformComponent>().Scale = glm::vec3(2.0f);

    constexpr uint32_t size = 128u;
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = size, .Height = size });
    EditorCamera frontCamera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
    EditorCamera backCamera(glm::vec3(0.0f, 0.0f, -4.0f), 0.0f, 90.0f);
    frontCamera.SetViewportSize(size, size);
    backCamera.SetViewportSize(size, size);
    const glm::vec3 samplePosition(-0.5f, -0.5f, 0.0f);

    const auto renderLuminance = [&](EditorCamera& camera) -> float
    {
        ctx.AdvanceFrames(
            3,
            [&](float)
            {
                scene->OnRenderEditor(sceneRenderer, camera);
            }
        );
        const glm::ivec2 pixel = ProjectToPixel(camera, samplePosition, size, size);
        return AverageLuminance(ReadRgba8(sceneRenderer->GetFinalImage()), pixel);
    };

    material->DoubleSided = false;
    const float singleSidedFront = renderLuminance(frontCamera);
    const float singleSidedBack = renderLuminance(backCamera);

    material->DoubleSided = true;
    const float doubleSidedFront = renderLuminance(frontCamera);
    const float doubleSidedBack = renderLuminance(backCamera);

    EXPECT_LT(glm::min(singleSidedFront, singleSidedBack), 0.02f);
    EXPECT_GT(glm::min(doubleSidedFront, doubleSidedBack), 0.05f);
}

TEST(Renderer, SceneRenderer_DebugDirectionalLightArrowFollowsTransformRotation)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    Ref<Scene> scene = ctx.GetScene();
    scene->GetEnvironmentSettings().ZenithColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().HorizonColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().GroundColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().AmbientIntensity = 0.0f;

    Entity sun = scene->CreateEntity("Sun");
    sun.AddComponent<DirectionalLightComponent>().Intensity = 0.0f;
    sun.GetComponent<TransformComponent>().Scale = glm::vec3(2.0f, 0.0f, -3.0f);

    constexpr uint32_t size = 256u;
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(
        scene,
        SceneRendererSpecification{
            .Width = size,
            .Height = size,
            .EnableDebugRendering = true,
        }
    );
    EditorCamera camera(glm::vec3(0.0f, 0.0f, 5.0f), 0.0f, -90.0f);
    camera.SetViewportSize(size, size);

    ctx.AdvanceFrames(
        2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );
    const Rgba8Readback down = ReadRgba8(sceneRenderer->GetFinalImage());
    const glm::ivec2 sunSample = ProjectToPixel(camera, glm::vec3(0.28f, 0.0f, 0.0f), size, size);
    const glm::ivec2 downSample = ProjectToPixel(camera, glm::vec3(0.0f, -0.75f, 0.0f), size, size);
    const glm::ivec2 rightSample = ProjectToPixel(camera, glm::vec3(0.75f, 0.0f, 0.0f), size, size);
    EXPECT_TRUE(MaxLuminance(down, sunSample, 1) > 0.3f);
    EXPECT_TRUE(MaxLuminance(down, downSample) > 0.3f);
    EXPECT_TRUE(MaxLuminance(down, rightSample) < 0.1f);

    sun.GetComponent<TransformComponent>().Rotation.z = glm::half_pi<float>();
    ctx.AdvanceFrames(
        2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );
    const Rgba8Readback right = ReadRgba8(sceneRenderer->GetFinalImage());
    EXPECT_TRUE(MaxLuminance(right, rightSample) > 0.3f);
    EXPECT_TRUE(MaxLuminance(right, downSample) < 0.1f);
}

TEST(Renderer, SceneRenderer_EquirectangularSkyMapsTopToPositiveY)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    const ActiveProjectRestorer restoreProject;
    const Testing::TempDir dir;
    const auto projectDirectory = dir.File("Project");
    std::filesystem::create_directories(projectDirectory / "Assets" / "Textures");
    Ref<AssetManager> assetManager = Ref<AssetManager>::Create();
    Project::New(ProjectSpecification{ .Name = "SkyOrientation", .ProjectDirectory = projectDirectory }, assetManager);

    const auto hdrPath = projectDirectory / "Assets" / "Textures" / "latitude.hdr";
    EP_REQUIRE(FS::WriteBytes(hdrPath, MakeLatitudeHdr(8u, 4u), true));
    Ref<Asset> asset = Ref<Asset>::Create();
    asset->Handle = AssetHandle(801);
    EP_REQUIRE(assetManager->CreateAsset(hdrPath, asset));

    Ref<Scene> scene = ctx.GetScene();
    scene->GetEnvironmentSettings().SkyboxHandle = AssetHandle(801);
    scene->GetBloomSettings().Intensity = 0.0f;

    constexpr uint32_t size = 64u;
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = size, .Height = size });
    EditorCamera upward(glm::vec3(0.0f), 89.0f, 0.0f);
    upward.SetViewportSize(size, size);
    ctx.AdvanceFrames(
        3,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, upward);
        }
    );
    const glm::vec3 upColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

    EditorCamera downward(glm::vec3(0.0f), -89.0f, 0.0f);
    downward.SetViewportSize(size, size);
    ctx.AdvanceFrames(
        2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, downward);
        }
    );
    const glm::vec3 downColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

    EditorCamera upperSide(glm::vec3(0.0f), 30.0f, 0.0f);
    upperSide.SetViewportSize(size, size);
    ctx.AdvanceFrames(
        2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, upperSide);
        }
    );
    const glm::vec3 upperSideColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

    EditorCamera lowerSide(glm::vec3(0.0f), -30.0f, 0.0f);
    lowerSide.SetViewportSize(size, size);
    ctx.AdvanceFrames(
        2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, lowerSide);
        }
    );
    const glm::vec3 lowerSideColor = ReadPixel(ReadRgba8(sceneRenderer->GetFinalImage()), glm::ivec2(size / 2u));

    EXPECT_TRUE(upColor.r > upColor.b + 0.25f);
    EXPECT_TRUE(downColor.b > downColor.r + 0.25f);
    EXPECT_TRUE(upperSideColor.r > upperSideColor.b + 0.25f);
    EXPECT_TRUE(lowerSideColor.b > lowerSideColor.r + 0.25f);
}

// Regression: the Tracy GPU context must be set up on its own command buffer, not by wrapping an
// nvrhi command list, which double-began/re-submitted the buffer and tripped Vulkan validation.
TEST(Renderer, VulkanGpuProfiler_Construction_EmitsNoVulkanValidationErrors)
{
    if (!Testing::AppHarness::IsAvailable())
        return;
    if (DeviceManager::Get()->GetParams().API != RendererAPI::Vulkan)
        GTEST_SKIP() << "This case checks Vulkan profiler construction.";

    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    const auto sink = std::make_shared<ErrorCountingSink>();
    Log::AddSink(sink);

    {
        const VulkanGpuProfiler profiler;
    }

    EXPECT_EQ(0u, sink->ErrorCount());
}

TEST(Renderer, Renderer_CompositeToSwapchain_SurvivesImageCyclingAndResize)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    Application* app = Testing::AppHarness::Get();
    Ref<Scene> scene = ctx.GetScene();
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
    const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);
    const uint32_t imageCount = app->GetDeviceManager()->GetBackBufferCount();
    uint32_t renderedFrames = 0;

    Ref<ImGuiLayer> imguiLayer = app->GetImGuiLayer();
    imguiLayer->SetClearMainSwapchainTarget(false);
    ctx.AdvanceFrames(
        imageCount + 2,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
            app->GetDeviceManager()->GetRenderer()->CompositeToSwapchain(sceneRenderer->GetFinalImage());
            renderedFrames++;
        }
    );

    glfwSetWindowSize(app->GetWindow()->GetNative(), 960, 540);
    ctx.AdvanceFrames(
        imageCount + 2,
        [&](float)
        {
            const auto [width, height] = app->GetWindow()->GetFramebufferSize();
            if (width > 0 && height > 0)
                sceneRenderer->Resize(width, height);
            scene->OnRenderEditor(sceneRenderer, camera);
            app->GetDeviceManager()->GetRenderer()->CompositeToSwapchain(sceneRenderer->GetFinalImage());
            renderedFrames++;
        }
    );

    EXPECT_EQ((imageCount + 2) * 2, renderedFrames);
    EXPECT_TRUE(app->IsRunning());
}

TEST(Renderer, SceneRenderer_ForwardFrameRendersSceneGeometry)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    Ref<Scene> scene = ctx.GetScene();
    auto& environment = scene->GetEnvironmentSettings();
    environment.ZenithColor = glm::vec3(0.0f);
    environment.HorizonColor = glm::vec3(0.0f);
    environment.GroundColor = glm::vec3(0.0f);
    environment.AmbientIntensity = 0.0f;

    Ref<AssetManager> assetManager = Project::GetActive()->GetAssetManager();
    Ref<Mesh> mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cube)).As<Mesh>();
    EP_REQUIRE(mesh != nullptr);
    mesh->GetMaterial(0)->EmissiveFactor = glm::vec3(2.0f, 1.0f, 0.5f);

    Entity cube = scene->CreateEntity("Cube");
    cube.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);

    constexpr uint32_t size = 128u;
    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = size, .Height = size });
    EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
    camera.SetViewportSize(size, size);

    ctx.AdvanceFrames(
        3,
        [&](float) -> void
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );

    const Rgba8Readback frame = ReadRgba8(sceneRenderer->GetFinalImage());
    const glm::ivec2 cubePixel = ProjectToPixel(camera, glm::vec3(0.0f, 0.0f, 0.0f), size, size);
    const glm::ivec2 skyPixel = ProjectToPixel(camera, glm::vec3(3.0f, 2.0f, -2.0f), size, size);
    EXPECT_GT(AverageLuminance(frame, cubePixel), 0.2f);
    EXPECT_LT(AverageLuminance(frame, skyPixel), 0.02f);
    EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
}

TEST(Renderer, SceneRenderer_ForwardTargetsRebindAfterResize)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    Ref<Scene> scene = ctx.GetScene();
    auto& environment = scene->GetEnvironmentSettings();
    environment.ZenithColor = glm::vec3(0.0f);
    environment.HorizonColor = glm::vec3(0.0f);
    environment.GroundColor = glm::vec3(0.0f);
    environment.AmbientIntensity = 0.0f;

    Ref<AssetManager> assetManager = Project::GetActive()->GetAssetManager();
    Ref<Mesh> mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cube)).As<Mesh>();
    EP_REQUIRE(mesh != nullptr);
    mesh->GetMaterial(0)->EmissiveFactor = glm::vec3(2.0f, 1.0f, 0.5f);

    Entity cube = scene->CreateEntity("Cube");
    cube.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);

    constexpr uint32_t initialWidth = 64u;
    constexpr uint32_t initialHeight = 64u;
    Ref<SceneRenderer> sceneRenderer =
        Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = initialWidth, .Height = initialHeight });
    EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
    camera.SetViewportSize(initialWidth, initialHeight);

    ctx.AdvanceFrames(
        2,
        [&](float) -> void
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );

    for (const auto dimensions : { glm::uvec2(96u, 48u), glm::uvec2(80u, 112u) })
    {
        const uint32_t resizedWidth = dimensions.x;
        const uint32_t resizedHeight = dimensions.y;
        sceneRenderer->Resize(resizedWidth, resizedHeight);
        camera.SetViewportSize(resizedWidth, resizedHeight);
        ctx.AdvanceFrames(
            2,
            [&](float) -> void
            {
                scene->OnRenderEditor(sceneRenderer, camera);
            }
        );

        // The cube must still come through the rebound forward/display-conversion
        // targets, not a cleared framebuffer.
        Ref<Image> finalImage = sceneRenderer->GetFinalImage();
        EP_REQUIRE(finalImage != nullptr);
        EXPECT_EQ(resizedWidth, finalImage->GetWidth());
        EXPECT_EQ(resizedHeight, finalImage->GetHeight());
        const Rgba8Readback frame = ReadRgba8(finalImage);
        const glm::ivec2 cubePixel = ProjectToPixel(camera, glm::vec3(0.0f, 0.0f, 0.0f), resizedWidth, resizedHeight);
        const glm::ivec2 skyPixel = ProjectToPixel(camera, glm::vec3(3.0f, 2.0f, -2.0f), resizedWidth, resizedHeight);
        EXPECT_GT(AverageLuminance(frame, cubePixel), 0.2f);
        EXPECT_LT(AverageLuminance(frame, skyPixel), 0.02f);
        EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
    }
}

TEST(Renderer, SceneRenderer_SetSceneInvalidatesOnlyOnIdentityChange)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    Ref<Scene> scene = ctx.GetScene();
    Ref<Scene> otherScene = Ref<Scene>::Create();
    auto& environment = scene->GetEnvironmentSettings();
    environment.ZenithColor = glm::vec3(0.0f);
    environment.HorizonColor = glm::vec3(0.0f);
    environment.GroundColor = glm::vec3(0.0f);
    environment.AmbientIntensity = 0.0f;

    Ref<AssetManager> assetManager = Project::GetActive()->GetAssetManager();
    Ref<Mesh> mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cube)).As<Mesh>();
    EP_REQUIRE(mesh != nullptr);
    mesh->GetMaterial(0)->EmissiveFactor = glm::vec3(2.0f, 1.0f, 0.5f);

    Ref<SceneRenderer> sceneRenderer = Ref<SceneRenderer>::Create(scene, SceneRendererSpecification{ .Width = 128u, .Height = 128u });
    EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, -90.0f);
    camera.SetViewportSize(128u, 128u);

    ctx.AdvanceFrames(
        1,
        [&](float) -> void
        {
            sceneRenderer->BeginScene(camera);
            sceneRenderer->SubmitEnvironmentSettings(environment);
            sceneRenderer->SubmitMesh(static_cast<uint64_t>(MeshPrimitiveType::Cube), glm::mat4(1.0f));
            sceneRenderer->SetScene(scene);
            sceneRenderer->EndScene();
        }
    );
    const glm::ivec2 cubePixel = ProjectToPixel(camera, glm::vec3(0.0f), 128u, 128u);
    EXPECT_GT(AverageLuminance(ReadRgba8(sceneRenderer->GetFinalImage()), cubePixel), 0.2f);

    ctx.AdvanceFrames(
        1,
        [&](float) -> void
        {
            sceneRenderer->BeginScene(camera);
            sceneRenderer->SubmitMesh(static_cast<uint64_t>(MeshPrimitiveType::Cube), glm::mat4(1.0f));
            sceneRenderer->SetScene(otherScene);
            sceneRenderer->SubmitEnvironmentSettings(environment);
            sceneRenderer->EndScene();
        }
    );
    EXPECT_LT(AverageLuminance(ReadRgba8(sceneRenderer->GetFinalImage()), cubePixel), 0.02f);
}
