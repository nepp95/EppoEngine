#include "pch.h"
#include "ImGuiLayer.h"

#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan_custom.h>
#include <glfw/glfw3.h>
#include <imgui_internal.h>

namespace Eppo
{
	static std::vector<VkCommandBuffer> s_ImGuiCmds;
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
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable multi-viewport

		ImGui::StyleColorsDark();

		// Initialize
		Ref<RendererContext> context = RendererContext::Get();
		ImGui_ImplGlfw_InitForVulkan(context->GetWindowHandle(), true);

		Ref<PhysicalDevice> physicalDevice = context->GetPhysicalDevice();
		Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();

		// Descriptors
		VkDescriptorPoolSize poolSizes[] =
		{
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
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 }
		};

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.poolSizeCount = IM_ARRAYSIZE(poolSizes);
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);

		VK_CHECK(vkCreateDescriptorPool(logicalDevice->GetNativeDevice(), &poolInfo, nullptr, &s_DescriptorPool), "Failed to create descriptor pool!");

		// Init
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = context->GetVulkanInstance();
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

		ImGui_ImplVulkan_Init(&initInfo, context->GetSwapchain()->GetRenderPass());

		// Fonts
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		context->WaitIdle();
		ImGui_ImplVulkan_DestroyFontUploadObjects();

		// ImGui commandbuffers
		s_ImGuiCmds.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			s_ImGuiCmds[i] = logicalDevice->GetSecondaryCommandBuffer();
	}

	void ImGuiLayer::OnDetach()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkDestroyDescriptorPool(device, s_DescriptorPool, nullptr);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{

	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::ImRender()
	{
		ImGui::Render();
	}

	void ImGuiLayer::End()
	{
		Renderer::SubmitCommand([]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer swapCmd = swapchain->GetCurrentRenderCommandBuffer();

			ImDrawData* data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(data, swapCmd);
		});
	}
}
