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

	private:
		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;

		std::vector<VkClearValue> m_ClearValues;
	};
}
