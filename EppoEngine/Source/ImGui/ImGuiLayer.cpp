#include "pch.h"
#include "ImGuiLayer.h"

#include "ImGui/Image.h"
#include "Platform/Vulkan/VulkanContext.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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

		constexpr float fontSize = 14.0f;
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
		const auto context = VulkanContext::Get();
		ImGui_ImplGlfw_InitForVulkan(context->GetWindowHandle(), true);

		const auto physicalDevice = context->GetPhysicalDevice();
		const auto logicalDevice = context->GetLogicalDevice();

		// Create descriptor pool
		const VkDescriptorPoolSize poolSizes[] = {
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

		VK_CHECK(vkCreateDescriptorPool(logicalDevice->GetNativeDevice(), &descriptorPoolCreateInfo, nullptr, &s_DescriptorPool), "Failed to create descriptor pool!")

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

		constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

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

		// Clear resources which in turn calls the vulkan API to clear all the descriptor sets allocated for ImGui
		UI::ClearResources();

		// Destroy other vulkan resources
		ImGui_ImplVulkan_Shutdown();

		// Finally destroy the descriptor pool - this needs to happen after ImGui_ImplVulkan_Shutdown because of the font texture
		vkDestroyDescriptorPool(device, s_DescriptorPool, nullptr);

		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		EPPO_PROFILE_FUNCTION("ImGuiLayer::OnEvent");

		if (m_BlockEvents)
		{
			const ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	static glm::vec4 ImGuiToGLM(const ImVec4& vec)
	{
		return { vec.x, vec.y, vec.z, vec.w };
	}

	static ImVec4 SRGBToLinear(const ImVec4& vec)
	{
		glm::vec4 v = ImGuiToGLM(vec);

		return glm::vec4(glm::pow(v.x, 2.2f), glm::pow(v.y, 2.2f), glm::pow(v.z, 2.2f), glm::pow(v.w, 2.2f));
	}

	void ImGuiLayer::SetupStyle() const
	{
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
		ImVec4* colors = ImGui::GetStyle().Colors;

		colors[ImGuiCol_WindowBg] = SRGBToLinear(ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f });
		colors[ImGuiCol_MenuBarBg] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });

		// Border
		colors[ImGuiCol_Border] = SRGBToLinear(ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f });
		colors[ImGuiCol_BorderShadow] = SRGBToLinear(ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f });

		// Text
		colors[ImGuiCol_Text] = SRGBToLinear(ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
		colors[ImGuiCol_TextDisabled] = SRGBToLinear(ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });

		// Headers
		colors[ImGuiCol_Header] = SRGBToLinear(ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f });
		colors[ImGuiCol_HeaderHovered] = SRGBToLinear(ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f });
		colors[ImGuiCol_HeaderActive] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });

		// Buttons
		colors[ImGuiCol_Button] = SRGBToLinear(ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f });
		colors[ImGuiCol_ButtonHovered] = SRGBToLinear(ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f });
		colors[ImGuiCol_ButtonActive] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_CheckMark] = SRGBToLinear(ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f });

		// Popups
		colors[ImGuiCol_PopupBg] = SRGBToLinear(ImVec4{ 0.1f, 0.1f, 0.13f, 0.92f });

		// Slider
		colors[ImGuiCol_SliderGrab] = SRGBToLinear(ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f });
		colors[ImGuiCol_SliderGrabActive] = SRGBToLinear(ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f });

		// Frame BG
		colors[ImGuiCol_FrameBg] = SRGBToLinear(ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f });
		colors[ImGuiCol_FrameBgHovered] = SRGBToLinear(ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f });
		colors[ImGuiCol_FrameBgActive] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });

		// Tabs
		colors[ImGuiCol_Tab] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_TabHovered] = SRGBToLinear(ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f });
		colors[ImGuiCol_TabActive] = SRGBToLinear(ImVec4{ 0.2f, 0.22f, 0.27f, 1.0f });
		colors[ImGuiCol_TabUnfocused] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_TabUnfocusedActive] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });

		// Title
		colors[ImGuiCol_TitleBg] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_TitleBgActive] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_TitleBgCollapsed] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = SRGBToLinear(ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f });
		colors[ImGuiCol_ScrollbarGrab] = SRGBToLinear(ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f });
		colors[ImGuiCol_ScrollbarGrabHovered] = SRGBToLinear(ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f });
		colors[ImGuiCol_ScrollbarGrabActive] = SRGBToLinear(ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f });

		// Seperator
		colors[ImGuiCol_Separator] = SRGBToLinear(ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f });
		colors[ImGuiCol_SeparatorHovered] = SRGBToLinear(ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f });
		colors[ImGuiCol_SeparatorActive] = SRGBToLinear(ImVec4{ 0.84f, 0.58f, 1.0f, 1.0f });

		// Resize Grip
		colors[ImGuiCol_ResizeGrip] = SRGBToLinear(ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f });
		colors[ImGuiCol_ResizeGripHovered] = SRGBToLinear(ImVec4{ 0.74f, 0.58f, 0.98f, 0.29f });
		colors[ImGuiCol_ResizeGripActive] = SRGBToLinear(ImVec4{ 0.84f, 0.58f, 1.0f, 0.29f });

		// Docking
		colors[ImGuiCol_DockingPreview] = SRGBToLinear(ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f });

		auto& style = ImGui::GetStyle();
		style.TabRounding = 4;
		style.ScrollbarRounding = 9;
		style.WindowRounding = 7;
		style.GrabRounding = 3;
		style.FrameRounding = 3;
		style.PopupRounding = 4;
		style.ChildRounding = 4;
	}
}
