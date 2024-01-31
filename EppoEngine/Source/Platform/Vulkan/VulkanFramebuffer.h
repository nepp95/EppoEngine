#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Framebuffer.h"

namespace Eppo
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const FramebufferSpecification& specification);
		virtual ~VulkanFramebuffer();

		virtual void Resize(uint32_t width, uint32_t height) override;

		VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }

		VkExtent2D GetExtent() const { return { GetWidth(), GetHeight() }; }
		const std::vector<VkClearValue>& GetClearValues() const { return m_ClearValues; }

		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

		Ref<Image> GetFinalImage() const override { return m_ImageAttachments[0]; }

		bool HasDepthAttachment() const override { return m_DepthTesting; }
		Ref<Image> GetDepthImage() const override { return m_DepthImage; }

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

	private:
		FramebufferSpecification m_Specification;

		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;

		std::vector<Ref<Image>> m_ImageAttachments;
		Ref<Image> m_DepthImage;

		bool m_DepthTesting = false;

		std::vector<VkClearValue> m_ClearValues;
	};
}
