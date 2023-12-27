#pragma once

#include "Renderer/Framebuffer.h"

namespace Eppo
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		virtual ~VulkanFramebuffer();

		virtual void Resize(uint32_t width, uint32_t height) override;

	private:
		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;
	};
}
