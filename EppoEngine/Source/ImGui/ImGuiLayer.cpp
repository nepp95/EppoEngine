#include "pch.h"
#include "ImGui/ImGuiLayer.h"

#include "Core/Application.h"
#include "Renderer/DeviceManager.h"

// TODO: TEMPORARY
#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Platform/Vulkan/Swapchain.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>

namespace Eppo
{
	struct ImGuiViewportData
	{
		bool WindowOwned = false;
		ScopedPtr<Swapchain> Swapchain = nullptr;
		ScopedPtr<ImGuiRenderer> Renderer = nullptr;
	};

	namespace
	{
		// Cohesive dark theme with a pumpkin-orange (#E8641C) accent. Applied on top
		// of StyleColorsDark so any unset color keeps a sensible dark default.
		auto SetEppoTheme() -> void
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImVec4* colors = style.Colors;

			const ImVec4 accent       = { 0.910f, 0.392f, 0.110f, 1.00f }; // #E8641C
			const ImVec4 accentHover  = { 0.961f, 0.475f, 0.227f, 1.00f }; // #F5793A
			const ImVec4 accentActive = { 0.788f, 0.325f, 0.082f, 1.00f }; // #C95315

			const ImVec4 bg0 = { 0.086f, 0.086f, 0.098f, 1.00f };
			const ImVec4 bg1 = { 0.125f, 0.125f, 0.141f, 1.00f };
			const ImVec4 bg2 = { 0.169f, 0.169f, 0.188f, 1.00f };
			const ImVec4 bg3 = { 0.220f, 0.220f, 0.243f, 1.00f };

			colors[ImGuiCol_Text]                  = { 0.90f, 0.90f, 0.92f, 1.00f };
			colors[ImGuiCol_TextDisabled]          = { 0.50f, 0.50f, 0.53f, 1.00f };
			colors[ImGuiCol_WindowBg]              = bg1;
			colors[ImGuiCol_ChildBg]               = { 0.00f, 0.00f, 0.00f, 0.00f };
			colors[ImGuiCol_PopupBg]               = bg0;
			colors[ImGuiCol_Border]                = { 0.00f, 0.00f, 0.00f, 0.35f };
			colors[ImGuiCol_BorderShadow]          = { 0.00f, 0.00f, 0.00f, 0.00f };
			colors[ImGuiCol_FrameBg]               = bg2;
			colors[ImGuiCol_FrameBgHovered]        = bg3;
			colors[ImGuiCol_FrameBgActive]         = { accentActive.x, accentActive.y, accentActive.z, 0.55f };
			colors[ImGuiCol_TitleBg]               = bg0;
			colors[ImGuiCol_TitleBgActive]         = bg0;
			colors[ImGuiCol_TitleBgCollapsed]      = bg0;
			colors[ImGuiCol_MenuBarBg]             = bg0;
			colors[ImGuiCol_ScrollbarBg]           = bg0;
			colors[ImGuiCol_ScrollbarGrab]         = bg3;
			colors[ImGuiCol_ScrollbarGrabHovered]  = { 0.35f, 0.35f, 0.38f, 1.00f };
			colors[ImGuiCol_ScrollbarGrabActive]   = accent;
			colors[ImGuiCol_CheckMark]             = accent;
			colors[ImGuiCol_SliderGrab]            = accent;
			colors[ImGuiCol_SliderGrabActive]      = accentHover;
			colors[ImGuiCol_Button]                = bg2;
			colors[ImGuiCol_ButtonHovered]         = accent;
			colors[ImGuiCol_ButtonActive]          = accentActive;
			colors[ImGuiCol_Header]                = { accent.x, accent.y, accent.z, 0.55f };
			colors[ImGuiCol_HeaderHovered]         = { accent.x, accent.y, accent.z, 0.75f };
			colors[ImGuiCol_HeaderActive]          = accent;
			colors[ImGuiCol_Separator]             = { 0.00f, 0.00f, 0.00f, 0.35f };
			colors[ImGuiCol_SeparatorHovered]      = accent;
			colors[ImGuiCol_SeparatorActive]       = accentHover;
			colors[ImGuiCol_ResizeGrip]            = { accent.x, accent.y, accent.z, 0.25f };
			colors[ImGuiCol_ResizeGripHovered]     = { accent.x, accent.y, accent.z, 0.60f };
			colors[ImGuiCol_ResizeGripActive]      = accent;
			colors[ImGuiCol_Tab]                   = bg1;
			colors[ImGuiCol_TabHovered]            = accent;
			colors[ImGuiCol_TabSelected]           = accentActive;
			colors[ImGuiCol_TabSelectedOverline]   = accentHover;
			colors[ImGuiCol_TabDimmed]             = bg1;
			colors[ImGuiCol_TabDimmedSelected]     = bg2;
			colors[ImGuiCol_DockingPreview]        = { accent.x, accent.y, accent.z, 0.70f };
			colors[ImGuiCol_DockingEmptyBg]        = bg0;
			colors[ImGuiCol_TextSelectedBg]        = { accent.x, accent.y, accent.z, 0.35f };
			colors[ImGuiCol_NavCursor]             = accent;
			colors[ImGuiCol_DragDropTarget]        = accentHover;

			style.WindowRounding    = 6.0f;
			style.ChildRounding     = 6.0f;
			style.FrameRounding     = 4.0f;
			style.PopupRounding     = 4.0f;
			style.GrabRounding      = 4.0f;
			style.TabRounding       = 4.0f;
			style.ScrollbarRounding = 4.0f;
			style.WindowBorderSize  = 1.0f;
			style.FrameBorderSize   = 0.0f;
			style.WindowPadding     = { 8.0f, 8.0f };
			style.FramePadding      = { 6.0f, 4.0f };
			style.ItemSpacing       = { 8.0f, 6.0f };
			style.ItemInnerSpacing  = { 6.0f, 4.0f };
			style.ScrollbarSize     = 12.0f;
			style.GrabMinSize       = 10.0f;
		}
	}

	auto ImGuiLayer::OnAttach() -> void
	{
		// Create imgui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		// Setup imgui config
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Load font
		constexpr float fontSize = 14.0f;
		const auto fontPath = FS::GetResourcesDirectory() / "Fonts" / "Roboto-Regular.ttf";
		io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), fontSize);

		// Setup style
		ImGui::StyleColorsDark();
		SetEppoTheme();

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Initialize imgui for glfw
		const auto& dm = DeviceManager::Get();
		const auto& window = Application::Get().GetWindow();

		if (dm->GetParams().API == RendererAPI::Vulkan)
			ImGui_ImplGlfw_InitForVulkan(window->GetNative(), true);
		else
			ImGui_ImplGlfw_InitForOther(window->GetNative(), true);

		// Create renderer
		m_ImGuiRenderer = CreateScopedPtr<ImGuiRenderer>();
		InitPlatformInterface();
	}

	auto ImGuiLayer::OnDetach() -> void
	{
		ImGui::DestroyPlatformWindows();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	auto ImGuiLayer::OnEvent(Event& e) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::OnEvent")

		if (m_BlockEvents)
		{
			const ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}

		if (e.GetEventType() == EventType::WindowResize)
			m_ImGuiRenderer->Resize();
	}

	auto ImGuiLayer::PrepareRender() -> void
	{
		EP_PROFILE_FN("ImGuiLayer::PrepareRender")

		m_ImGuiRenderer->UpdateFontTexture();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	auto ImGuiLayer::Render() -> void
	{
		EP_PROFILE_FN("ImGuiLayer::Render")

		const auto& dm = static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());

		ImGui::Render();
		m_ImGuiRenderer->RenderToSwapchain(ImGui::GetMainViewport(), dm->GetSwapchain());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	auto ImGuiLayer::BlockEvents(bool blockEvents) -> void
	{
		m_BlockEvents = blockEvents;
	}

	auto ImGuiLayer::InitPlatformInterface() -> void
	{
		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			EP_ASSERT(platformIO.Platform_CreateVkSurface != nullptr);

		platformIO.Renderer_CreateWindow = ImGuiRenderer_CreateWindow;
		platformIO.Renderer_DestroyWindow = ImGuiRenderer_DestroyWindow;
		platformIO.Renderer_SetWindowSize = ImGuiRenderer_SetWindowSize;
		platformIO.Renderer_RenderWindow = ImGuiRenderer_RenderWindow;
		platformIO.Renderer_SwapBuffers = ImGuiRenderer_SwapBuffers;
	}

	auto ImGuiLayer::ImGuiRenderer_CreateWindow(ImGuiViewport* viewport) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::ImGuiRenderer_CreateWindow")

		const auto& dm = static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

		ImGuiViewportData* data = IM_NEW(ImGuiViewportData)();
		viewport->RendererUserData = data;

		VkSurfaceKHR surface = nullptr;
		VkInstance instance = dm->GetVulkanInstance();
		VK_CHECK(platformIO.Platform_CreateVkSurface(viewport, reinterpret_cast<ImU64>(instance), nullptr, reinterpret_cast<ImU64*>(&surface)), "Failed to create vk surface for ImGui!");
		
		data->Swapchain = CreateScopedPtr<Swapchain>(surface);
		data->Swapchain->CreateSwapchain(static_cast<uint32_t>(viewport->Size.x), static_cast<uint32_t>(viewport->Size.y));
		data->Renderer = CreateScopedPtr<ImGuiRenderer>();
		data->WindowOwned = true;
	}

	auto ImGuiLayer::ImGuiRenderer_DestroyWindow(ImGuiViewport* viewport) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::ImGuiRenderer_DestroyWindow")

		ImGuiViewportData* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData);
		IM_DELETE(vd);
		viewport->RendererUserData = nullptr;
	}

	auto ImGuiLayer::ImGuiRenderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::ImGuiRenderer_SetWindowSize")

		ImGuiViewportData* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData);
		vd->Swapchain->Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
	}

	auto ImGuiLayer::ImGuiRenderer_RenderWindow(ImGuiViewport* viewport, void*) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::ImGuiRenderer_RenderWindow")

		ImGuiViewportData* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData);
		vd->Swapchain->BeginFrame();
		vd->Renderer->UpdateFontTexture();
		vd->Renderer->RenderToSwapchain(viewport, vd->Swapchain);
	}

	auto ImGuiLayer::ImGuiRenderer_SwapBuffers(ImGuiViewport* viewport, void*) -> void
	{
		EP_PROFILE_FN("ImGuiLayer::ImGuiRenderer_SwapBuffers")

		ImGuiViewportData* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData);
		vd->Swapchain->Present();
	}
}