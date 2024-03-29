#pragma once

#include "Renderer/Texture.h"

namespace Eppo
{
	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		RGBA8,
		
		// Depth
		DEPTH24STENCIL8,
		Depth = DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format)
		{}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
	};

	struct FramebufferSpecification
	{
		FramebufferSpecification() = default;
		FramebufferSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments)
		{}

		std::vector<FramebufferTextureSpecification> Attachments;
		Ref<Texture> ExistingDepthTexture;

		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 1;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& specification);
		~Framebuffer();

		void Invalidate();

		void RT_Bind() const;
		void RT_Unbind() const;

		void Cleanup();
		void Resize(uint32_t width, uint32_t height);

		uint32_t GetColorAttachmentID() const { return m_ColorAttachments[0]; }
		uint32_t GetDepthAttachmentID() const { return m_DepthAttachment; }

		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

	private:
		FramebufferSpecification m_Specification;
		uint32_t m_RendererID;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecs;
		FramebufferTextureSpecification m_DepthAttachmentSpec;

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment;
	};
}
