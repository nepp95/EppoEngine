#include "pch.h"
#include "Framebuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	namespace Utils
	{
		static std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT };
		static VkFormat FindSupportedDepthFormat()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<PhysicalDevice> physicalDevice = context->GetPhysicalDevice();

			for (const auto& format : depthFormats)
			{
				VkFormatProperties properties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice->GetNativeDevice(), format, &properties);
				if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return format;
			}
		}
	}

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

		m_ImageAttachments.clear();

		for (const auto& attachment : m_Specification.Attachments)
		{
			if (Utils::IsDepthFormat(attachment))
				continue;

			ImageSpecification imageSpec;
			imageSpec.Format = attachment;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.Width = GetWidth();
			imageSpec.Height = GetHeight();

			m_ImageAttachments.emplace_back(CreateRef<Image>(imageSpec));

			VkAttachmentDescription& colorAttachment = attachmentDescriptions.emplace_back();
			colorAttachment.format = Utils::ImageFormatToVkFormat(attachment);
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = m_Specification.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = m_Specification.Clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorAttachmentRef = attachmentReferences.emplace_back(VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		}

		VkSubpassDescription& subpassDescription = subpassDescriptions.emplace_back();
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = (uint32_t)attachmentReferences.size();
		subpassDescription.pColorAttachments = attachmentReferences.data();

		if (!m_ImageAttachments.empty())
		{
			VkSubpassDependency& subpassDependency = subpassDependencies.emplace_back();
			subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependency.dstSubpass = 0;
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.srcAccessMask = 0;
			subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
