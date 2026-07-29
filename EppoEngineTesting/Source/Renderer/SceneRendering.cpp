#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TestContext.h"
#include "Support/TempDir.h"

#include "Asset/AssetManager.h"
#include "Project/Project.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

#include <GLFW/glfw3.h>

using namespace Eppo;

namespace
{
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
}

// End-to-end rendering over real frames: these drive a Scene through the SceneRenderer
// on the booted graphical harness, so they need a display + GPU.
SUITE(Renderer)
{
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

        scene->GetEnvironment().AmbientIntensity = 0.75f;

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
        scene->GetEnvironment().SkyboxHandle = AssetHandle(800);

        // A mesh so the geometry pass samples the baked irradiance/prefilter/LUT too.
        Entity sphere = scene->CreateEntity("Sphere");
        sphere.AddComponent<MeshComponent>().MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Sphere);

        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 128u, .Height = 128u });
        const EditorCamera camera(glm::vec3(0.0f, 0.0f, 4.0f), 0.0f, 0.0f);

        // The bake fires on the first frame's SubmitEnvironment; rendering several frames sends the
        // baked cubes/LUT through both the geometry and skybox passes under the validation layer.
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        const Ref<Image>& finalImage = sceneRenderer->GetFinalImage();
        REQUIRE CHECK(finalImage != nullptr);
        CHECK_EQUAL(128u, finalImage->GetWidth());
        CHECK(Testing::AppHarness::Get()->IsRunning());

        Project::SetActive(previous);
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
