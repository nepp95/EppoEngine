#include "pch.h"
#include "Application.h"

#include "Renderer/Renderer.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Application::Application");

		// Set instance if not set. We can only have one instance!
		EPPO_ASSERT(!s_Instance);
		s_Instance = this;

		// Set working directory
		if (!m_Specification.WorkingDirectory.empty())
			std::filesystem::current_path(m_Specification.WorkingDirectory);

		// Create window
		WindowSpecification windowSpec;
		windowSpec.Width = m_Specification.WindowWidth;
		windowSpec.Height = m_Specification.WindowHeight;
		windowSpec.Title = m_Specification.Name;

		m_Window = CreateScope<Window>(windowSpec);
		m_Window->Init();
		m_Window->SetEventCallback([this](Event& e) { Application::OnEvent(e);  });

		// Initialize systems
		Renderer::Init();

		// Add GUI layer
		m_ImGuiLayer = new ImGuiLayer();
		PushLayer(m_ImGuiLayer, true);
	}

	Application::~Application()
	{
		EPPO_PROFILE_FUNCTION("Application::~Application");
		EPPO_INFO("Shutting down...");

		Renderer::Shutdown();

		for (Layer* layer : m_LayerStack)
			layer->OnDetach();

		m_Window->Shutdown();
	}

	void Application::Close()
	{
		EPPO_PROFILE_FUNCTION("Application::Close");

		m_IsRunning = false;
	}

	void Application::OnEvent(Event& e)
	{
		EPPO_PROFILE_FUNCTION("Application::OnEvent");

		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));

		for (Layer* layer : m_LayerStack)
		{
			if (e.Handled)
				break;

			layer->OnEvent(e);
		}
	}

	void Application::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("Application::RenderGui");

		Renderer::SubmitCommand([this]()
		{
			m_ImGuiLayer->Begin();

			for (Layer* layer : m_LayerStack)
				layer->RenderGui();

			m_ImGuiLayer->ImRender();
		});

		Renderer::BeginRenderPass();
		m_ImGuiLayer->End();
		Renderer::EndRenderPass();
	}

	void Application::PushLayer(Layer* layer, bool overlay)
	{
		EPPO_PROFILE_FUNCTION("Application::PushLayer");

		if (overlay)
			m_LayerStack.PushOverlay(layer);
		else
			m_LayerStack.PushLayer(layer);
	}

	void Application::PopLayer(Layer* layer, bool overlay)
	{
		EPPO_PROFILE_FUNCTION("Application::PopLayer");

		if (overlay)
			m_LayerStack.PopOverlay(layer);
		else
			m_LayerStack.PopLayer(layer);
	}

	void Application::Run()
	{
		while (m_IsRunning)
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			{
				EPPO_PROFILE_FUNCTION("CPU Update");

				float time = (float)glfwGetTime();
				float timestep = time - m_LastFrameTime;
				m_LastFrameTime = time;

				m_Window->ProcessEvents();

				for (Layer* layer : m_LayerStack)
					layer->Update(timestep);
			}

			if (!m_IsMinimized)
			{
				{
					EPPO_PROFILE_FUNCTION("CPU Prepare Render");

					// 1. Start command buffer
					Renderer::BeginFrame();

					// 2. Record commands
					for (Layer* layer : m_LayerStack)
						layer->Render();

					RenderGui();

					// 3. End command buffer
					Renderer::EndFrame();
				}
				{
					// 4. Execute all of the above between beginning the swapchain frame and presenting it (Render queue)
					EPPO_PROFILE_FUNCTION("CPU Render");
					swapchain->BeginFrame();
					Renderer::ExecuteRenderCommands();
				}
				{
					EPPO_PROFILE_FUNCTION("CPU Wait");
					swapchain->Present();
				}
			}

			EPPO_PROFILE_FRAME_MARK;
		}

		RendererContext::Get()->WaitIdle();
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		EPPO_PROFILE_FUNCTION("Application::OnWindowClose");

		Close();

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		EPPO_PROFILE_FUNCTION("Application::OnWindowResize");
		EPPO_ASSERT(false);

		return true;
	}
}
