#include "pch.h"
#include "ImGuiLayer.h"

#include "Platform/OpenGL/UI/OpenGLImGuiLayer.h"
#include "Platform/Vulkan/UI/VulkanImGuiLayer.h"
#include "Renderer/RendererAPI.h"

#include <imgui_internal.h>

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

		ImGui::StyleColorsDark();

		// Initialize
		InitAPI();
	}

	void ImGuiLayer::OnDetach()
	{
		DestroyAPI();

		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	Ref<ImGuiLayer> ImGuiLayer::Create()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLImGuiLayer>::Create().As<ImGuiLayer>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanImGuiLayer>::Create().As<ImGuiLayer>();
			}
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
