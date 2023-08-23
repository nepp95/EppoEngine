#include "pch.h"
#include "ImGuiLayer.h"

#include "Renderer/Framebuffer.h"
#include "Renderer/RendererContext.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glfw/glfw3.h>
#include <imgui_internal.h>

namespace Eppo
{
	static std::vector<VkCommandBuffer> s_ImGuiCmds;
	static VkDescriptorPool s_DescriptorPool = nullptr;
	static VkRenderPass s_RenderPass = nullptr;

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

		// Render pass
		FramebufferSpecification framebufferSpec;

		/*VkAttachmentDescription colorAttachment{};
		colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &subpassDependency;

		VK_CHECK(vkCreateRenderPass(logicalDevice->GetNativeDevice(), &renderPassInfo, nullptr, &s_RenderPass), "Failed to create render pass!");*/

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

	void ImGuiLayer::End()
	{
		ImGui::Render();

		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();
		VkCommandBuffer swapCmd = swapchain->GetCurrentRenderCommandBuffer();

		VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

		uint32_t width = swapchain->GetWidth();
		uint32_t height = swapchain->GetHeight();

		/*VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;*/

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = swapchain->GetRenderPass();
		renderPassBeginInfo.framebuffer = swapchain->GetCurrentFramebuffer();
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = { width, height };
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(swapCmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		VkCommandBufferInheritanceInfo inheritanceInfo{};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.renderPass = swapchain->GetRenderPass();
		inheritanceInfo.framebuffer = swapchain->GetCurrentFramebuffer();

		VkCommandBufferBeginInfo cmdBeginInfoImGui{};
		cmdBeginInfoImGui.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBeginInfoImGui.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		cmdBeginInfoImGui.pInheritanceInfo = &inheritanceInfo;

		VK_CHECK(vkBeginCommandBuffer(s_ImGuiCmds[imageIndex], &cmdBeginInfoImGui), "Failed to begin secondary command buffer!");

		ImDrawData* data = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(data, s_ImGuiCmds[imageIndex]);

		VK_CHECK(vkEndCommandBuffer(s_ImGuiCmds[imageIndex]), "Failed to end secondary command buffer!");

		vkCmdExecuteCommands(swapCmd, 1, &s_ImGuiCmds[imageIndex]);
		vkCmdEndRenderPass(swapCmd);
	}
}
