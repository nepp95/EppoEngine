#include "pch.h"
#include "ImGuiLayer.h"

#include "Core/Application.h"
#include "Renderer/Renderer.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

namespace Eppo
{
	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{}

	void ImGuiLayer::OnAttach()
	{
		// Configure ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable multi-viewport

		float fontSize = 14.0f;
		io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/DroidSans.ttf", fontSize);

		ImGui::StyleColorsDark();
		SetupStyle();

		// When viewports are enabled we tweak WindowRounding so platform windows can look identical
		ImGuiStyle& style = ImGui::GetStyle();

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init(nullptr);
	}

	void ImGuiLayer::OnDetach()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		EPPO_PROFILE_FUNCTION("ImGuiLayer::OnEvent");

		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::Begin() const
	{
		EPPO_PROFILE_FUNCTION("ImGuiLayer::Begin");

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::End() const
	{
		EPPO_PROFILE_FUNCTION("ImGuiLayer::End");

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backupContext = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backupContext);
		}
	}

	void ImGuiLayer::SetupStyle() const
	{
		ImGuiStyle& style = ImGui::GetStyle();
		
		EPPO_TRACE("{}: {}", "Alpha", style.Alpha);
		EPPO_TRACE("{}: {}", "CellPadding", (glm::vec2)style.CellPadding);
		EPPO_TRACE("{}: {}", "ChildRounding", style.ChildRounding);
		EPPO_TRACE("{}: {}", "ColumnsMinSpacing", style.ColumnsMinSpacing);
		EPPO_TRACE("{}: {}", "DockingSeparatorSize", style.DockingSeparatorSize);
		EPPO_TRACE("{}: {}", "FrameBorderSize", style.FrameBorderSize);
		EPPO_TRACE("{}: {}", "FramePadding", (glm::vec2)style.FramePadding);
		EPPO_TRACE("{}: {}", "FrameRounding", style.FrameRounding);
		EPPO_TRACE("{}: {}", "HoverDelayNormal", style.HoverDelayNormal);
		EPPO_TRACE("{}: {}", "HoverDelayShort", style.HoverDelayShort);
		EPPO_TRACE("{}: {}", "HoverStationaryDelay", style.HoverStationaryDelay);
		EPPO_TRACE("{}: {}", "IndentSpacing", style.IndentSpacing);
		EPPO_TRACE("{}: {}", "ItemInnerSpacing", (glm::vec2)style.ItemInnerSpacing);
		EPPO_TRACE("{}: {}", "ItemSpacing", (glm::vec2)style.ItemSpacing);
		EPPO_TRACE("{}: {}", "PopupBorderSize", style.PopupBorderSize);
		EPPO_TRACE("{}: {}", "PopupRounding", style.PopupRounding);
		EPPO_TRACE("{}: {}", "ScrollbarRounding", style.ScrollbarRounding);
		EPPO_TRACE("{}: {}", "ScrollbarSize", style.ScrollbarSize);
		EPPO_TRACE("{}: {}", "TabBorderSize", style.TabBorderSize);
		EPPO_TRACE("{}: {}", "TabRounding", style.TabRounding);
		EPPO_TRACE("{}: {}", "WindowBorderSize", style.WindowBorderSize);
		EPPO_TRACE("{}: {}", "WindowMinSize", (glm::vec2)style.WindowMinSize);
		EPPO_TRACE("{}: {}", "WindowPadding", (glm::vec2)style.WindowPadding);
		EPPO_TRACE("{}: {}", "WindowRounding", style.WindowRounding);

		// Colors
		auto colors = style.Colors;
		
		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}
}
