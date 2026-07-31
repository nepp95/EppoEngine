#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TestContext.h"

#include "Core/Log.h"
#include "Platform/Vulkan/VulkanGpuProfiler.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/DeviceManager.h"
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
