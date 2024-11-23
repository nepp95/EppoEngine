#include "pch.h"
#include "ImGuiLayer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

namespace Eppo
{
	static std::vector<VkCommandBuffer> s_ImGuiCommandBuffers;
	static VkDescriptorPool s_DescriptorPool = nullptr;

	static void CheckVkResult(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[Vulkan][ImGui] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

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
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

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

		// Init
		Ref<VulkanContext> context = VulkanContext::Get();
		ImGui_ImplGlfw_InitForVulkan(context->GetWindowHandle(), true);

		Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();
		Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();

		// Create descriptor pool
		VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptorPoolCreateInfo.poolSizeCount = IM_ARRAYSIZE(poolSizes);
		descriptorPoolCreateInfo.pPoolSizes = poolSizes;
		descriptorPoolCreateInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);

		VK_CHECK(vkCreateDescriptorPool(logicalDevice->GetNativeDevice(), &descriptorPoolCreateInfo, nullptr, &s_DescriptorPool), "Failed to create descriptor pool!");

		// Init
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = VulkanContext::GetVulkanInstance();
		initInfo.PhysicalDevice = physicalDevice->GetNativeDevice();
		initInfo.Device = logicalDevice->GetNativeDevice();
		initInfo.QueueFamily = physicalDevice->GetQueueFamilyIndices().Graphics;
		initInfo.Queue = logicalDevice->GetGraphicsQueue();
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = s_DescriptorPool;
		initInfo.Subpass = 0;
		initInfo.MinImageCount = VulkanConfig::MaxFramesInFlight;
		initInfo.ImageCount = VulkanConfig::MaxFramesInFlight;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = CheckVkResult;
		initInfo.UseDynamicRendering = true;

		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

		VkPipelineRenderingCreateInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderInfo.colorAttachmentCount = 1;
		renderInfo.pColorAttachmentFormats = &format;

		initInfo.PipelineRenderingCreateInfo = renderInfo;

		ImGui_ImplVulkan_Init(&initInfo);

		// Fonts
		ImGui_ImplVulkan_CreateFontsTexture();

		// Command buffers
		s_ImGuiCommandBuffers.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			s_ImGuiCommandBuffers[i] = logicalDevice->GetSecondaryCommandBuffer();

	}

	void ImGuiLayer::OnDetach()
	{
		VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkDestroyDescriptorPool(device, s_DescriptorPool, nullptr);

		ImGui_ImplVulkan_Shutdown();
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

	void ImGuiLayer::SetupStyle() const
	{
		ImGuiStyle& style = ImGui::GetStyle();
		
		/*EPPO_TRACE("{}: {}", "Alpha", style.Alpha);
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
		EPPO_TRACE("{}: {}", "WindowRounding", style.WindowRounding);*/

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

	void ImGuiLayer::SetupStyle() const
	{
		ImGuiStyle& style = ImGui::GetStyle();
		
		/*EPPO_TRACE("{}: {}", "Alpha", style.Alpha);
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
		EPPO_TRACE("{}: {}", "WindowRounding", style.WindowRounding);*/

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
