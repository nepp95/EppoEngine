#include "pch.h"
#include "VulkanRenderer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"

namespace Eppo
{
	bool VulkanRenderer::s_IsInstantiated = false;

	VulkanRenderer::VulkanRenderer()
	{
		EPPO_ASSERT(!s_IsInstantiated);
		s_IsInstantiated = true;

		const Ref<VulkanContext> context = VulkanContext::Get();
		const Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
		const Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

		// Create descriptor allocators
		const std::vector<DescriptorAllocator::PoolSizeRatio> ratios = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				3.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			3.0f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			3.0f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	4.0f }
		};

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			m_DescriptorAllocators[i].Init(1000, ratios);

		// Load shaders
		constexpr std::array shaders = {
			"Resources/Shaders/composite.glsl",
			"Resources/Shaders/debug.glsl",
			"Resources/Shaders/env.glsl",
			"Resources/Shaders/geometry.glsl",
			"Resources/Shaders/predepth.glsl",
			"Resources/Shaders/skybox.glsl"
		};

#ifdef EPPO_DEBUG
		std::for_each(std::execution::seq, shaders.cbegin(), shaders.cend(), [&](const std::string& path)
		{
			m_ShaderLibrary.Load(path);
		});
#elif defined(EPPO_RELEASE)
		std::for_each(std::execution::par, shaders.cbegin(), shaders.cend(), [&](const std::string& path)
		{
			m_ShaderLibrary.Load(path);
		});
#endif
	}

	void VulkanRenderer::Shutdown()
	{
		for (auto& allocator : m_DescriptorAllocators)
		{
			EPPO_MEM_WARN("Releasing descriptor pool {}", static_cast<void*>(&allocator));
			allocator.DestroyPools();
		}
	}

	void VulkanRenderer::ExecuteRenderCommands()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::ExecuteRenderCommands");

		m_CommandQueue.Execute();
	}

	void VulkanRenderer::SubmitCommand(RenderCommand command)
	{
		m_CommandQueue.AddCommand(std::move(command));
	}

	void VulkanRenderer::BeginRenderPass(const Ref<CommandBuffer>& commandBuffer, const Ref<Pipeline>& pipeline)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::BeginRenderPass");

		const Ref<VulkanContext> context = VulkanContext::Get();
		const Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

		const auto& spec = pipeline->GetSpecification();

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = { spec.Width, spec.Height };
		renderingInfo.layerCount = 1;

		std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
		VkRenderingAttachmentInfo depthAttachmentInfo{};

		if (spec.SwapchainTarget)
		{
			VkRenderingAttachmentInfo& attachmentInfo = colorAttachmentInfos.emplace_back();
			attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentInfo.imageView = swapchain->GetCurrentImageView();
			attachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &attachmentInfo;
		} else
		{
			bool depth = false;

			for (const auto& attachment : spec.RenderAttachments)
			{
				if (const auto& image = attachment.RenderImage; 
					image->GetSpecification().Format == ImageFormat::Depth)
				{
					depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
					depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					depthAttachmentInfo.loadOp = attachment.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
					depthAttachmentInfo.imageView = std::static_pointer_cast<VulkanImage>(image)->GetImageInfo().ImageView;
					depthAttachmentInfo.clearValue.depthStencil = { attachment.ClearValue.Depth, 0 };
					depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

					depth = true;
				} else
				{
					VkRenderingAttachmentInfo& attachmentInfo = colorAttachmentInfos.emplace_back();
					attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
					attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachmentInfo.loadOp = attachment.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
					attachmentInfo.imageView = std::static_pointer_cast<VulkanImage>(image)->GetImageInfo().ImageView;
					attachmentInfo.clearValue.color = { attachment.ClearValue.Color.r, attachment.ClearValue.Color.g, attachment.ClearValue.Color.b, attachment.ClearValue.Color.a };
					attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
			}

			renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size());
			renderingInfo.pColorAttachments = colorAttachmentInfos.data();

			if (depth)
				renderingInfo.pDepthAttachment = &depthAttachmentInfo;

			if (spec.CubeMap)
				renderingInfo.viewMask = 0b111111;
		}

		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
		const VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
		vkCmdBeginRendering(cb, &renderingInfo);
	}

	void VulkanRenderer::EndRenderPass(const Ref<CommandBuffer>& commandBuffer)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::EndRenderPass");

		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
		const VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
		vkCmdEndRendering(cb);
	}

	void* VulkanRenderer::AllocateDescriptor(void* layout)
	{
		const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
		return m_DescriptorAllocators[frameIndex].Allocate(layout);
	}
}
