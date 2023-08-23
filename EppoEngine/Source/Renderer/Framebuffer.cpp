#include "pch.h"
#include "Framebuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	Framebuffer::Framebuffer(const FramebufferSpecification& specification)
		: m_Specification(specification)
	{
		std::vector<Ref<Image>> imageAttachments;
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		std::vector<VkSubpassDescription> subpassDescriptions;
		std::vector<VkSubpassDependency> subpassDependencies;

		for (const auto& attachment : m_Specification.Attachments)
		{
			if (Utils::IsDepthFormat(attachment))
				continue;

			ImageSpecification imageSpec;
			imageSpec.Format = attachment;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.Width = GetWidth();
			imageSpec.Height = GetHeight();

			imageAttachments.emplace_back(CreateRef<Image>(imageSpec));

			VkAttachmentDescription& colorAttachment = attachmentDescriptions.emplace_back();
			colorAttachment.format = Utils::ImageFormatToVkFormat(attachment);
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription& subpassDescription = subpassDescriptions.emplace_back();
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency& subpassDependency = subpassDependencies.emplace_back();
			subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependency.dstSubpass = 0;
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.srcAccessMask = 0;
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = attachmentDescriptions.size();
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.subpassCount = subpassDescriptions.size();
		renderPassInfo.pSubpasses = subpassDescriptions.data();
		renderPassInfo.dependencyCount = subpassDependencies.size();
		renderPassInfo.pDependencies = subpassDependencies.data();

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass), "Failed to create render pass!");

		std::vector<VkImageView> attachments(imageAttachments.size());
		for (size_t i = 0; i < imageAttachments.size(); i++)
			attachments[i] = imageAttachments[i]->GetImageView();

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = GetWidth();
		framebufferInfo.height = GetHeight();
		framebufferInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer), "Failed to create framebuffer!");
	}
}
