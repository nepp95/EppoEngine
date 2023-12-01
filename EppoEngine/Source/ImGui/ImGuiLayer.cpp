#include "pch.h"
#include "ImGuiLayer.h"

#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan_custom.h>
#include <GLFW/glfw3.h>
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
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable multi-viewport

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
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::End()
	{
		ImGui::Render();

		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
		VkCommandBuffer swapCmd = swapchain->GetCurrentRenderCommandBuffer();

		VkClearValue clearValue = { 0.1f, 0.1f, 0.1f, 1.0f };
		uint32_t width = swapchain->GetWidth();
		uint32_t height = swapchain->GetHeight();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		VkCommandBufferBeginInfo swapCmdBeginInfo{};
		swapCmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		swapCmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		swapCmdBeginInfo.pNext = nullptr;

		VK_CHECK(vkBeginCommandBuffer(swapCmd, &swapCmdBeginInfo), "Failed to begin command buffer!");

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = swapchain->GetRenderPass();
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchain->GetExtent();
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;
		renderPassBeginInfo.framebuffer = swapchain->GetCurrentFramebuffer();

		vkCmdBeginRenderPass(swapCmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		VkCommandBufferInheritanceInfo inheritanceInfo{};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.renderPass = swapchain->GetRenderPass();
		inheritanceInfo.framebuffer = swapchain->GetCurrentFramebuffer();

		VkCommandBufferBeginInfo imGuiCmdInfo{};
		imGuiCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		imGuiCmdInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		imGuiCmdInfo.pInheritanceInfo = &inheritanceInfo;

		VK_CHECK(vkBeginCommandBuffer(s_ImGuiCmds[imageIndex], &imGuiCmdInfo), "Failed to begin command buffer!");

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = (float)height;
		viewport.height = (float)height;
		viewport.width = (float)width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(s_ImGuiCmds[imageIndex], 0, 1, &viewport);

		VkRect2D scissor;
		scissor.extent = { width, height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(s_ImGuiCmds[imageIndex], 0, 1, &scissor);

		ImDrawData* data = ImGui::GetDrawData();
		//ImGui_ImplVulkan_RenderDrawData(data, swapCmd);
		ImGui_ImplVulkan_RenderDrawData(data, s_ImGuiCmds[imageIndex]);

		VK_CHECK(vkEndCommandBuffer(s_ImGuiCmds[imageIndex]), "Failed to end command buffer!");

		vkCmdExecuteCommands(swapCmd, 1, &s_ImGuiCmds[imageIndex]);
		vkCmdEndRenderPass(swapCmd);

		VK_CHECK(vkEndCommandBuffer(swapCmd), "Failed to end command buffer!");

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}
