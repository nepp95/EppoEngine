#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TestContext.h"

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

#include <GLFW/glfw3.h>

using namespace Eppo;

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

        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
        const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);

        // Render across several real frames to cycle the frames-in-flight indices.
        ctx.AdvanceFrames(
            3,
            [&](float)
            {
                scene->OnRenderEditor(sceneRenderer, camera);
            }
        );

        CHECK(sceneRenderer->GetFinalImage() != nullptr);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(Renderer_CompositeToSwapchain_SurvivesImageCyclingAndResize)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        Application* app = Testing::AppHarness::Get();
        const Ref<Scene> scene = ctx.GetScene();
        const Ref<SceneRenderer> sceneRenderer =
            CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
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

        CHECK_EQUAL((imageCount + 2) * 2, renderedFrames);
        CHECK(app->IsRunning());
    }
}
