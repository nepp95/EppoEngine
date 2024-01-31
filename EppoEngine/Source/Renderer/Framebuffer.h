#pragma once

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

		uint32_t Width = 0;
		uint32_t Height = 0;

		bool ClearColorOnLoad = true;
		glm::vec4 ClearColor = { 0.5f, 0.5f, 0.5f, 1.0f };

		bool ClearDepthOnLoad = false;
		float ClearDepth = 1.0f;
	};

	class Framebuffer : public RefCounter
	{
	public:
		virtual ~Framebuffer() {};

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual Ref<Image> GetFinalImage() const = 0;

		virtual bool HasDepthAttachment() const = 0;
		virtual Ref<Image> GetDepthImage() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& specification);
	};
}
