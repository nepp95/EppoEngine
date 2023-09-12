#include "pch.h"
#include "Framebuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	Framebuffer::Framebuffer(const FramebufferSpecification& specification)
		: m_Specification(specification)
	{
		Create();
	}

	Framebuffer::~Framebuffer()
	{
		Cleanup();
	}

	void Framebuffer::Create()
	{
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		std::vector<VkAttachmentReference> attachmentReferences;
		std::vector<VkSubpassDescription> subpassDescriptions;
		std::vector<VkSubpassDependency> subpassDependencies;
		VkAttachmentReference depthAttachmentReference;

		m_ImageAttachments.clear();

		uint32_t index = 0;
		for (const auto& attachment : m_Specification.Attachments)
		{
			ImageSpecification imageSpec;
			imageSpec.Format = attachment;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.Width = GetWidth();
			imageSpec.Height = GetHeight();

			m_ImageAttachments.emplace_back(CreateRef<Image>(imageSpec));

			if (Utils::IsDepthFormat(attachment))
			{
				VkAttachmentDescription& depthAttachment = attachmentDescriptions.emplace_back();
				depthAttachment.format = Utils::FindSupportedDepthFormat(); // TODO: Is it possible this will defer between calls?
				depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				depthAttachment.loadOp = m_Specification.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthAttachment.initialLayout = m_Specification.Clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				depthAttachmentReference.attachment = index;
				depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				m_DepthTesting = true;
			}
			else
			{
				VkAttachmentDescription& colorAttachment = attachmentDescriptions.emplace_back();
				colorAttachment.format = Utils::ImageFormatToVkFormat(attachment);
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachment.loadOp = m_Specification.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = m_Specification.Clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkAttachmentReference colorAttachmentRef = attachmentReferences.emplace_back(VkAttachmentReference{ index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			}
			index++;
		}

		VkSubpassDescription& subpassDescription = subpassDescriptions.emplace_back();
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = (uint32_t)attachmentReferences.size();
		subpassDescription.pColorAttachments = attachmentReferences.data();

		if (m_DepthTesting)
			subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

		if (!m_ImageAttachments.empty())
		{
			{
				VkSubpassDependency& subpassDependency = subpassDependencies.emplace_back();
				subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				subpassDependency.dstSubpass = 0;
				subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				subpassDependency.srcAccessMask = 0;
				subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			}

			if (m_DepthTesting)
			{
				VkSubpassDependency& subpassDependency = subpassDependencies.emplace_back();
				subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				subpassDependency.dstSubpass = 0;
				subpassDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				subpassDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				subpassDependency.srcAccessMask = 0;
				subpassDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			}
		}

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = (uint32_t)attachmentDescriptions.size();
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.subpassCount = (uint32_t)subpassDescriptions.size();
		renderPassInfo.pSubpasses = subpassDescriptions.data();
		renderPassInfo.dependencyCount = (uint32_t)subpassDependencies.size();
		renderPassInfo.pDependencies = subpassDependencies.data();

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass), "Failed to create render pass!");

		std::vector<VkImageView> attachments(m_ImageAttachments.size());
		for (size_t i = 0; i < m_ImageAttachments.size(); i++)
			attachments[i] = m_ImageAttachments[i]->GetImageView();

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.attachmentCount = (uint32_t)attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = GetWidth();
		framebufferInfo.height = GetHeight();
		framebufferInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer), "Failed to create framebuffer!");

		// Clear values
		if (m_Specification.Clear)
		{
			const glm::vec4& color = m_Specification.ClearColor;

			VkClearValue colorClear;
			colorClear.color = { color.r, color.g, color.b, color.a };
			m_ClearValues.push_back(colorClear);
		}

		if (m_DepthTesting)
		{
			VkClearValue depthClear;
			depthClear.depthStencil = { 1.0f, 0 };
			m_ClearValues.push_back(depthClear);
		}
	}

	void Framebuffer::Cleanup()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		vkDestroyRenderPass(device, m_RenderPass, nullptr);
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Specification.Width = width;
		m_Specification.Height = height;

		Cleanup();
		Create();
	}
}
