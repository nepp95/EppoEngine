#include "pch.h"
#include "Application.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
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
	}

	Application::~Application()
	{
		EPPO_INFO("Shutting down...");

		for (Layer* layer : m_LayerStack)
			layer->OnDetach();

		m_Window->Shutdown();
	}

	void Application::Close()
	{
		m_IsRunning = false;
	}

	void Application::PushLayer(Layer* layer, bool overlay)
	{
		if (overlay)
			m_LayerStack.PushOverlay(layer);
		else
			m_LayerStack.PushLayer(layer);
	}

	void Application::PopLayer(Layer* layer, bool overlay)
	{
		if (overlay)
			m_LayerStack.PopOverlay(layer);
		else
			m_LayerStack.PopLayer(layer);
	}

	void Application::Run()
	{
		while (m_IsRunning)
		{
			float time = (float)glfwGetTime();
			float timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			for (Layer* layer : m_LayerStack)
				layer->Update(timestep);

			if (!m_IsMinimized)
			{
				for (Layer* layer : m_LayerStack)
					layer->Render();
			}
		}
	}
}
