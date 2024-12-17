#include "pch.h"
#include "Application.h"

#include "Core/Filesystem.h"
#include "Renderer/Renderer.h"
#include "Scripting/ScriptEngine.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	Application* Application::s_Instance = nullptr;

	Application::Application(ApplicationSpecification specification)
		: m_Specification(std::move(specification))
	{
		// Set instance if not set. We can only have one instance!
		EPPO_ASSERT(!s_Instance)
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
		Filesystem::Init();
		Renderer::Init();
		ScriptEngine::Init();

		// Add GUI layer
		m_ImGuiLayer = new ImGuiLayer();
		PushLayer(m_ImGuiLayer, true);
	}

	Application::~Application()
	{
		EPPO_INFO("Shutting down...");

		for (Layer* layer : m_LayerStack)
			layer->OnDetach();

		ScriptEngine::Shutdown();
		Renderer::Shutdown();
		m_Window->Shutdown();
	}

	void Application::Close()
	{
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

	void Application::SubmitToMainThread(const std::function<void()>& fn)
	{
		EPPO_PROFILE_FUNCTION("Application::SubmitToMainThread");

		std::scoped_lock<std::mutex> lock(m_MainThreadMutex);

		m_MainThreadQueue->AddCommand(fn);
	}

	void Application::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("Application::RenderGui");

		for (Layer* layer : m_LayerStack)
			layer->RenderGui();
	}

	void Application::PushLayer(Layer* layer, const bool overlay)
	{
		EPPO_PROFILE_FUNCTION("Application::PushLayer");

		if (overlay)
			m_LayerStack.PushOverlay(layer);
		else
			m_LayerStack.PushLayer(layer);
	}

	void Application::PopLayer(Layer* layer, const bool overlay)
	{
		EPPO_PROFILE_FUNCTION("Application::PopLayer");

		if (overlay)
			m_LayerStack.PopOverlay(layer);
		else
			m_LayerStack.PopLayer(layer);
	}

	void Application::Run()
	{
		Ref<RendererContext> context = RendererContext::Get();

		while (m_IsRunning)
		{
			auto time = static_cast<float>(glfwGetTime());
			float timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			ExecuteMainThreadQueue();

			{
				EPPO_PROFILE_FUNCTION("CPU Update");

				for (Layer* layer : m_LayerStack)
					layer->Update(timestep);
			}

			if (!m_IsMinimized)
			{
				{
					EPPO_PROFILE_FUNCTION("CPU Render");

					for (Layer* layer : m_LayerStack)
						layer->Render();
				}

				context->BeginFrame();
				Renderer::ExecuteRenderCommands();
				context->PresentFrame();

				EPPO_PROFILE_FRAME_MARK;
			}

			m_Window->ProcessEvents();
		}

		context->WaitIdle();
	}

	void Application::ExecuteMainThreadQueue()
	{
		EPPO_PROFILE_FUNCTION("Application::ExecuteMainThreadQueue");

		std::scoped_lock<std::mutex> lock(m_MainThreadMutex);

		m_MainThreadQueue->Execute();
	}

	bool Application::OnWindowClose(const WindowCloseEvent& e)
	{
		Close();

		return true;
	}

	bool Application::OnWindowResize(const WindowResizeEvent& e)
	{
		EPPO_PROFILE_FUNCTION("Application::OnWindowResize");

		uint32_t width = e.GetWidth();
		uint32_t height = e.GetHeight();

		if (width == 0 || height == 0)
		{
			m_IsMinimized = true;
			return false;
		}
			
		m_IsMinimized = false;

		return true;
	}
}
