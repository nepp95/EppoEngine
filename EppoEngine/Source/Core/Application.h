#pragma once

#include "Core/LayerStack.h"
#include "Core/Window.h"
#include "Debug/Profiler.h"
#include "Event/ApplicationEvent.h"
#include "ImGui/ImGuiLayer.h"

#include <string>

int main(int argc, char** argv);

namespace Eppo
{
	struct ApplicationCommandLineArgs
	{
		ApplicationCommandLineArgs() = default;
		ApplicationCommandLineArgs(int argc, char** argv)
			: Count(argc), Args(argv)
		{}

		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			EPPO_ASSERT((index < Count));
			if (index >= Count)
				return "";
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Client";
		std::string WorkingDirectory;

		uint32_t WindowWidth = 1600;
		uint32_t WindowHeight = 900;

		ApplicationCommandLineArgs CommandLineArgs;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& specification);
		~Application();

		void Close();
		void OnEvent(Event& e);

		void RenderGui();

		void PushLayer(Ref<Layer> layer, bool overlay = false);
		void PopLayer(Ref<Layer> layer, bool overlay = false);

		Ref<Profiler> GetProfiler() const { return m_Profiler; }

		static Application& Get() { return *s_Instance; }
		Window& GetWindow() const { return *m_Window; }
		Ref<ImGuiLayer> GetImGuiLayer() const { return m_ImGuiLayer; }

	private:
		void Run();

		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

	private:
		ApplicationSpecification m_Specification;

		Scope<Window> m_Window;
		LayerStack m_LayerStack;

		Ref<ImGuiLayer> m_ImGuiLayer;
		Ref<Profiler> m_Profiler;

		bool m_IsRunning = true;
		bool m_IsMinimized = false;
		float m_LastFrameTime = 0;

		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	// To be implemented by the client using this library
	Application* CreateApplication(ApplicationCommandLineArgs args);
}
