#pragma once

#include "Renderer/Allocator.h"
#include "Renderer/Image.h"

namespace Eppo
{
	struct FramebufferSpecification
	{
		FramebufferSpecification() = default;
		FramebufferSpecification(std::initializer_list<ImageFormat> attachments)
			: Attachments(attachments)
		{}

		std::vector<ImageFormat> Attachments;

		uint32_t Width;
		uint32_t Height;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& specification);

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

	private:
		FramebufferSpecification m_Specification;

		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;
	};
}
