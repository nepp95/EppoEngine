#include "pch.h"
#include "Application.h"

#include "Core/Filesystem.h"
#include "Renderer/Renderer.h"
#include "Scripting/ScriptEngine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tracy/TracyOpenGL.hpp>

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
		Filesystem::Init();
		Renderer::Init();
		ScriptEngine::Init();

		// Add GUI layer
		m_ImGuiLayer = new ImGuiLayer();
		PushLayer(m_ImGuiLayer, true);
	}

	Application::~Application()
	{
		EPPO_PROFILE_FUNCTION("Application::~Application");

		EPPO_INFO("Shutting down...");

		ScriptEngine::Shutdown();
		Renderer::Shutdown();

		for (Layer* layer : m_LayerStack)
			layer->OnDetach();

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
		std::scoped_lock<std::mutex> lock(m_MainThreadMutex);

		m_MainThreadQueue.push(fn);
	}

	void Application::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("Application::RenderGui");

		m_ImGuiLayer->Begin();

		for (Layer* layer : m_LayerStack)
			layer->RenderGui();

		m_ImGuiLayer->End();
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

			float time = (float)glfwGetTime();
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
					
					Renderer::SubmitCommand([this]() { RenderGui();	});
				}

				Renderer::ExecuteRenderCommands();
				m_Window->ProcessEvents();
				m_Window->SwapBuffers();
			}

			EPPO_PROFILE_GPU_END;
			EPPO_PROFILE_FRAME_MARK;
		}
	}

	void Application::ExecuteMainThreadQueue()
	{
		std::scoped_lock<std::mutex> lock(m_MainThreadMutex);

		for (size_t i = 0; i < m_MainThreadQueue.size(); i++)
		{
			m_MainThreadQueue.front()();
			m_MainThreadQueue.pop();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		Close();

		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		EPPO_PROFILE_FUNCTION("Application::OnWindowResize");
		
		uint32_t width = e.GetWidth();
		uint32_t height = e.GetHeight();

		if (width == 0 || height == 0)
		{
			m_IsMinimized = true;
			return false;
		}
		else
			m_IsMinimized = false;

		return true;
	}
}
