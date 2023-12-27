#pragma once

#include "Renderer/Vulkan.h"
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

		bool ClearColorOnLoad = true;
		glm::vec4 ClearColor = { 0.5f, 0.5f, 0.5f, 1.0f };

		bool ClearDepthOnLoad = false;
		float ClearDepth = 1.0f;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& specification);
		~Framebuffer();

		void Create();
		void Cleanup();
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

		Ref<Image> GetFinalImage() { return m_ImageAttachments[0]; };

		VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		VkExtent2D GetExtent() const { return { GetWidth(), GetHeight() }; }

		const std::vector<VkClearValue>& GetClearValues() const { return m_ClearValues; }

		bool HasDepthAttachment() const { return m_DepthTesting; }
		Ref<Image> GetDepthImage() { return m_DepthImage; }

		static Ref<Framebuffer> Create(const FramebufferSpecification& specification);

	private:
		FramebufferSpecification m_Specification;

		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;
		std::vector<Ref<Image>> m_ImageAttachments;
		Ref<Image> m_DepthImage;

		std::vector<VkClearValue> m_ClearValues;

		bool m_DepthTesting = false;
	};
}
