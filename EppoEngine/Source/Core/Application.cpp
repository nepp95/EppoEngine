#include "pch.h"
#include "Core/Application.h"

#include "ImGui/ImGuiLayer.h"
#include "Renderer/DeviceManager.h"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

namespace Eppo
{
    Application* Application::s_Instance = nullptr;

    Application::Application(ApplicationParams&& params)
        : m_Params(std::move(params))
    {
        EP_ASSERT(!s_Instance, "There can only be one instance of the application!");
        s_Instance = this;

        // Create window
        m_Window = CreateRef<Window>(WindowSpecification{
            .Title = m_Params.Title,
            .Width = m_Params.Width,
            .Height = m_Params.Height,
            .Fullscreen = m_Params.Fullscreen,
            .Decorated = m_Params.Decorated,
        });

        m_Window->SetEventCallback(
            [this](Event& e) -> void
            {
                OnEvent(e);
            }
        );

        // Create device manager (dx11/dx12/vk)
        const DeviceParams deviceParams{
            .API = RendererAPI::Vulkan,
            .VSync = m_Params.VSync,
        };

        m_DeviceManager = DeviceManager::Create(m_Window, deviceParams);
        m_DeviceManager->Init();

        m_ThreadPool = CreateRef<ThreadPool>();

        m_DeviceManager->InitRenderer();

        // A deployed runtime hands over the shaders it read from its game package; the editor and tests
        // pass none and compile from Resources/Shaders. Loading happens once, before ImGui takes one.
        m_DeviceManager->GetRenderer()->LoadShaders(m_Params.PackedShaders, m_Params.PackedShaderIncludes);

        // Create UI layer
        if (m_Params.EnableImGui)
            m_ImGuiLayer = PushLayer<ImGuiLayer>();
    }

    Application::~Application()
    {
        Log::Info("Application shutting down...");

        m_ImGuiLayer.reset();

        for (auto it = m_LayerStack.begin(); it != m_LayerStack.end();)
        {
            const auto layer = *it;
            layer->OnDetach();
            it = m_LayerStack.erase(it);
        }

        m_ThreadPool->Shutdown(true);
        m_DeviceManager->Shutdown();
        m_Window->Shutdown();

        // Release the singleton so a subsequent Application can be constructed in
        // the same process (e.g. a test harness that boots, tears down, re-boots).
        s_Instance = nullptr;
    }

    auto Application::Run() -> void
    {
        while (m_IsRunning)
        {
            const auto time = static_cast<float>(glfwGetTime());
            const float timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;

            StepFrame(timestep);
        }

        m_DeviceManager->GetDevice()->waitForIdle();
    }

    auto Application::StepFrame(const float timestep) const -> void
    {
        EP_PROFILE_FN("Application::StepFrame")

        m_Window->ProcessEvents();
        m_ThreadPool->Flush();

        if (!m_IsMinimized && m_DeviceManager->BeginFrame())
        {
            // Render work
            for (const auto& layer : m_LayerStack)
                layer->OnUpdate(timestep);

            if (m_ImGuiLayer)
            {
                m_ImGuiLayer->PrepareRender();

                for (const auto& layer : m_LayerStack)
                    layer->OnUIRender();

                m_ImGuiLayer->Render();
            }

            // Present
            m_DeviceManager->Present();
        }

        m_DeviceManager->GetDevice()->runGarbageCollection();
        EP_FRAME_MARK;
    }

    auto Application::OnEvent(Event& e) -> void
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(std::bind_front(&Application::OnWindowClose, this));
        dispatcher.Dispatch<WindowResizeEvent>(std::bind_front(&Application::OnWindowResize, this));

        for (const auto& layer : m_LayerStack)
        {
            if (e.Handled)
                break;

            layer->OnEvent(e);
        }
    }

    auto Application::OnWindowClose(const WindowCloseEvent& e) -> bool
    {
        Close();

        return true;
    }

    auto Application::OnWindowResize(const WindowResizeEvent& e) -> bool
    {
        const uint32_t width = e.GetWidth();
        const uint32_t height = e.GetHeight(); // NOLINT

        if (width == 0 || height == 0)
        {
            m_IsMinimized = true;
            return false;
        }

        m_IsMinimized = false;

        return false;
    }
}
