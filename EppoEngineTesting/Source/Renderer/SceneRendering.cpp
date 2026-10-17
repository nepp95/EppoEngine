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
#include "Scene/SceneSerializer.h"

#include <GLFW/glfw3.h>
#include <spdlog/sinks/base_sink.h>
#include <stb_image_write.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>

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

        auto RegisterMesh(const uint64_t handle, const std::string& filename, const std::string& source) -> AssetHandle
        {
            const std::filesystem::path path = m_ProjectDirectory / "Assets" / filename;
            EP_REQUIRE(FS::WriteText(path, source, true));

            const Ref<Asset> asset = CreateRef<Asset>();
            asset->Handle = AssetHandle(handle);
            EP_REQUIRE(m_AssetManager->CreateAsset(path, asset));
            return asset->Handle;
        }

        [[nodiscard]] auto Manager() const -> const Ref<AssetManager>& { return m_AssetManager; }

    private:
        Ref<Project> m_Previous;
        Testing::TempDir m_Directory;
        std::filesystem::path m_ProjectDirectory;
        Ref<AssetManager> m_AssetManager;
    };

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

    [[nodiscard]] auto ReadRgba8(const Ref<Image>& image) -> Rgba8Readback
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

}

// End-to-end rendering over real frames: these drive a Scene through the SceneRenderer
// on the booted graphical harness, so they need a display + GPU.
TEST(Renderer, SceneRenderer_GradientSky_RendersWithoutError)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    const Ref<Scene> scene = ctx.GetScene();

    const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
    const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);

    // Render across several real frames to cycle the frames-in-flight indices.
    ctx.AdvanceFrames(
        3,
        [&](float)
        {
            scene->OnRenderEditor(sceneRenderer, camera);
        }
    );

    EXPECT_TRUE(sceneRenderer->GetFinalImage() != nullptr);
    EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
}

TEST(Renderer, SceneRenderer_DoubleSidedMaterialRendersFromBothSides)
{
    Testing::TestContext ctx;
    if (!ctx.IsAvailable())
        return;

    PrimitiveProjectFixture project;
    const AssetHandle triangleHandle = project.RegisterMesh(700u, "DoubleSidedTriangle.gltf", MakeDoubleSidedTriangleGltf());
    const Ref<Mesh> triangleMesh = project.Manager()->GetOrLoadAsset<Mesh>(triangleHandle);
    EP_REQUIRE(triangleMesh != nullptr);
    const Ref<Material>& material = triangleMesh->GetMaterial(0);
    EP_REQUIRE(material != nullptr);

    const Ref<Scene> scene = ctx.GetScene();
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
    const Ref<SceneRenderer> sceneRenderer =
        CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = size, .Height = size });
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
    const Ref<Scene> scene = ctx.GetScene();
    scene->GetEnvironmentSettings().ZenithColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().HorizonColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().GroundColor = glm::vec3(0.0f);
    scene->GetEnvironmentSettings().AmbientIntensity = 0.0f;

    Entity sun = scene->CreateEntity("Sun");
    sun.AddComponent<DirectionalLightComponent>().Intensity = 0.0f;
    sun.GetComponent<TransformComponent>().Scale = glm::vec3(2.0f, 0.0f, -3.0f);

    constexpr uint32_t size = 256u;
    const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(
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
    const Ref<Scene> scene = ctx.GetScene();
    const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
    const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);
    const uint32_t imageCount = app->GetDeviceManager()->GetBackBufferCount();
    uint32_t renderedFrames = 0;

    app->GetImGuiLayer()->SetClearMainSwapchainTarget(false);
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
