#include "RuntimeLayer.h"

#include <imgui.h>

namespace Eppo
{
    auto RuntimeLayer::OnAttach() -> void
    {
        const auto rootDirectory = FS::GetExecutableDirectory();
        const AssetHandle startScene = m_GameData.StartScene;
        const std::string projectName = m_GameData.ProjectName;

        // The engine shaders were already loaded from m_GameData during application startup.
        m_AssetManager = CreateRef<AssetManager>(std::move(m_GameData.AssetRegistry), std::move(m_GameData.PackedAssets));
        m_Project = Project::New(
            ProjectSpecification{
                .Name = projectName,
                .ProjectDirectory = rootDirectory,
                .StartScene = startScene,
            },
            m_AssetManager
        );

        const bool scriptingReady = ScriptEngine::Init(rootDirectory / "runtimeconfig.json");
        m_ScriptEngineInitialized = ScriptEngine::IsInitialized();
        if (!scriptingReady)
        {
            Fail("The managed runtime could not be initialized from runtimeconfig.json.");
            return;
        }

        if (const auto userAssembly = rootDirectory / (projectName + ".dll"); FS::Exists(userAssembly))
        {
            if (!ScriptEngine::Get().LoadUserAssembly(userAssembly))
            {
                Fail(std::format("The game script assembly '{}' could not be loaded.", userAssembly.filename().string()));
                return;
            }
        }
        else
        {
            Log::Info("Game '{}' has no user script assembly.", projectName);
        }

        m_Scene = m_AssetManager->GetOrLoadAsset<Scene>(startScene);
        if (!m_Scene)
        {
            Fail("The packed start scene could not be loaded.");
            return;
        }
        if (!m_Scene->GetPrimaryCameraEntity())
        {
            Fail("The packed start scene does not contain a primary camera.");
            return;
        }

        const auto [width, height] = Application::Get().GetWindow()->GetFramebufferSize();
        if (width == 0 || height == 0)
        {
            Fail("The runtime window has no drawable framebuffer.");
            return;
        }
        m_SceneRenderer = CreateRef<SceneRenderer>(m_Scene, SceneRendererSpecification{ .Width = width, .Height = height });
        Resize(width, height);
        Input::SetViewportInputEnabled(true);

        if (const auto& imguiLayer = Application::Get().GetImGuiLayer())
            imguiLayer->SetClearMainSwapchainTarget(false);

        m_Scene->OnRuntimeStart();
        m_RuntimeStarted = true;
        m_StartupComplete = true;
    }

    auto RuntimeLayer::OnDetach() -> void
    {
        if (m_RuntimeStarted && m_Scene)
            m_Scene->OnRuntimeStop();
        m_RuntimeStarted = false;
        m_StartupComplete = false;

        m_SceneRenderer.reset();
        m_Scene.reset();

        if (m_ScriptEngineInitialized)
            ScriptEngine::Shutdown();
        m_ScriptEngineInitialized = false;

        Project::SetActive(nullptr);
        m_Project.reset();
        m_AssetManager.reset();
    }

    auto RuntimeLayer::OnUpdate(const float timestep) -> void
    {
        if (!m_StartupComplete)
            return;

        m_Scene->OnUpdateRuntime(timestep);
        m_Scene->OnRenderRuntime(m_SceneRenderer);
        Application::Get().GetDeviceManager()->GetRenderer()->CompositeToSwapchain(m_SceneRenderer->GetFinalImage());
    }

#if !defined(EP_DIST)
    auto RuntimeLayer::OnUIRender() -> void
    {
        if (!m_StartupComplete || !m_ShowOverlay)
            return;

        ImGui::Begin("Runtime");
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Frame: %.2f ms", 1000.0f / io.Framerate);
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::End();
        m_SceneRenderer->RenderGui();
    }
#endif

    auto RuntimeLayer::OnEvent(Event& event) -> void
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(std::bind_front(&RuntimeLayer::OnWindowResize, this));
        dispatcher.Dispatch<KeyPressedEvent>(std::bind_front(&RuntimeLayer::OnKeyPressed, this));
    }

    auto RuntimeLayer::OnWindowResize(const WindowResizeEvent&) const -> bool
    {
        if (!m_StartupComplete)
            return false;
        const auto [width, height] = Application::Get().GetWindow()->GetFramebufferSize();
        if (width > 0 && height > 0)
            Resize(width, height);
        return false;
    }

    auto RuntimeLayer::OnKeyPressed(const KeyPressedEvent& event) -> bool
    {
        if (event.IsRepeat())
            return false;
        if (event.GetKeyCode() == Key::Escape)
        {
            Application::Get().Close();
            return true;
        }

#if !defined(EP_DIST)
        if (event.GetKeyCode() != Key::F3)
            return false;
        m_ShowOverlay = !m_ShowOverlay;
        return true;
#else
        return false;
#endif
    }

    auto RuntimeLayer::Resize(const uint32_t width, const uint32_t height) const -> void
    {
        m_Scene->SetViewportSize(width, height);
        m_SceneRenderer->Resize(width, height);
    }

    auto RuntimeLayer::Fail(const std::string& message) -> void
    {
        Log::Error("Runtime startup failed: {}", message);
        ErrorDialog::Show("Eppo Runtime Error", message);
        Application::Get().Close();
    }
}
