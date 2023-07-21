#include "pch.h"
#include "Application.h"

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
	}

	void Application::Close()
	{
		m_IsRunning = false;
	}

	void Application::Run()
	{
		while (m_IsRunning)
		{

		}
	}
}