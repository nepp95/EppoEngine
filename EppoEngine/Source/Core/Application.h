#pragma once

#include "Core/Layer.h"
#include "Core/Window.h"
#include "Event/ApplicationEvent.h"
#include "ImGui/ImGuiLayer.h"
#include "Renderer/DeviceManager.h"

auto main(int argc, char** argv) -> int;

namespace Eppo
{
    struct CommandLineArgs
	{
		int Argc;
		char** Argv;

		CommandLineArgs(const int argc, char** argv)
			: Argc(argc), Argv(argv)
		{}

		auto operator[](const int index) const -> const char*
		{
			EP_ASSERT(index < Argc);
			if (index >= Argc)
				return "";
			return Argv[index];
		}
	};

	struct ApplicationParams
	{
		CommandLineArgs Args;

		std::string Title = "EppoEngine";
		uint32_t Width = 1600;
		uint32_t Height = 900;
		bool VSync = false;
		bool Fullscreen = false;
		bool Decorated = true;
		bool EnableImGui = true;
		bool EnableFileDialogs = true;
	};

	class Application
	{
	public:
        explicit Application(ApplicationParams&& params);
		virtual ~Application();

        /// Signals to the application that an application close is requested. This frame will end and then initiate shutdown.
        auto Close() -> void { m_IsRunning = false; }

	    /// Run the application
		auto Run() -> void;

		// Advance the application by exactly one frame with the given timestep:
		// pump window events, update layers, render, and present. Run() is just a
		// loop over this with a wall-clock timestep; a test harness can instead
		// drive frames deterministically (fixed timestep, controlled count).
		auto StepFrame(float timestep) const -> void;

		[[nodiscard]] auto IsRunning() const -> bool { return m_IsRunning; }

		auto OnEvent(Event& e) -> void;

		template<typename T>
			requires(std::derived_from<T, Layer>)
		auto PushLayer() -> Ref<T>
		{
			Ref<T> layer = CreateRef<T>();
			m_LayerStack.emplace_back(layer);
			layer->OnAttach();
			return layer;
		}

		[[nodiscard]] constexpr auto GetParams() const -> const ApplicationParams& { return m_Params; }
		[[nodiscard]] constexpr auto GetWindow() const -> const Ref<Window>& { return m_Window; }
		[[nodiscard]] constexpr auto GetDeviceManager() const -> const Ref<DeviceManager>& { return m_DeviceManager; }
		[[nodiscard]] constexpr auto GetImGuiLayer() const -> const Ref<ImGuiLayer>& { return m_ImGuiLayer; }

		static auto Get() -> Application& { return *s_Instance; }

	protected:
		ApplicationParams m_Params;

	private:
		auto OnWindowClose(const WindowCloseEvent& e) -> bool;
		auto OnWindowResize(const WindowResizeEvent& e) -> bool;

	private:
		Ref<Window> m_Window = nullptr;
		Ref<DeviceManager> m_DeviceManager = nullptr;

		std::vector<Ref<Layer>> m_LayerStack;
		Ref<ImGuiLayer> m_ImGuiLayer = nullptr;

		float m_LastFrameTime = 0.0f;
		bool m_IsRunning = true;
		bool m_IsMinimized = false;

		static Application* s_Instance;
		friend auto ::main(int argc, char** argv) -> int;
	};

	// IMPORTANT: To be implemented by the application!
	auto CreateApplication(int argc, char** argv) -> Application*;
}
