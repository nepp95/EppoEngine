#include "pch.h"
#include "Framebuffer.h"

#include <glad/glad.h>

namespace Eppo
{
	namespace Utils
	{
		static GLenum TextureTarget(bool multisampled)
		{
			return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}

		static void CreateTextures(bool multisampled, uint32_t* outID, uint32_t count)
		{
			glCreateTextures(TextureTarget(multisampled), count, outID);
		}

		static void BindTexture(bool multisampled, uint32_t id)
		{
			glBindTexture(TextureTarget(multisampled), id);
		}

		static void AttachColorTexture(uint32_t id, uint32_t samples, GLenum internalFormat, GLenum dataFormat, uint32_t width, uint32_t height, uint32_t index)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			} else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, uint32_t samples, GLenum dataFormat, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, dataFormat, width, height, GL_FALSE);
			} else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, dataFormat, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}

		static bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:	return true;
			}

			return false;
		}

		static GLenum TextureFormatToGLFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::RGBA8:		return GL_RGBA8;
			}

			EPPO_ASSERT(false);
			return 0;
		}
	}

	Framebuffer::Framebuffer(const FramebufferSpecification& specification)
		: m_Specification(specification)
	{
		for (auto specification : m_Specification.Attachments)
		{
			if (Utils::IsDepthFormat(specification.TextureFormat))
				m_ColorAttachmentSpecs.emplace_back(specification);
			else
				m_DepthAttachmentSpec = specification;
		}

		Invalidate();
	}

	Framebuffer::~Framebuffer()
	{
		Cleanup();
	}

	void Framebuffer::Invalidate()
	{
		if (m_RendererID)
		{
			Cleanup();

			m_ColorAttachments.clear();
			m_DepthAttachment = 0;
		}

		glCreateFramebuffers(1, &m_RendererID);
		Bind();

		bool multisampled = m_Specification.Samples > 1;

		// Color attachments
		if (m_ColorAttachmentSpecs.size())
		{
			m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());
			Utils::CreateTextures(multisampled, m_ColorAttachments.data(), m_ColorAttachments.size());

			for (size_t i = 0; i < m_ColorAttachments.size(); i++)
			{
				Utils::BindTexture(multisampled, m_ColorAttachments[i]);

				switch (m_ColorAttachmentSpecs[i].TextureFormat)
				{
					case FramebufferTextureFormat::RGBA8:
					{
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, i);
						break;
					}
				}
			}
		}

		// Depth attachment
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
		{
			Utils::CreateTextures(multisampled, &m_DepthAttachment, 1);
			Utils::BindTexture(multisampled, m_DepthAttachment);

			switch (m_DepthAttachmentSpec.TextureFormat)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:
				{
					Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
					break;
				}
			}
		}

		if (m_ColorAttachments.size() > 1)
		{
			EPPO_ASSERT(m_ColorAttachments.size() <= 4);
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			glDrawBuffers(m_ColorAttachments.size(), buffers);
		} else if (m_ColorAttachments.empty())
		{
			glDrawBuffer(GL_NONE);
		}

		EPPO_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		Unbind();
	}

	void Framebuffer::Bind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void Framebuffer::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::Cleanup()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			EPPO_WARN("Attempted to resize framebuffer to {}, {}. Operation canceled.", width, height);
			return;
		}

		m_Specification.Width = width;
		m_Specification.Height = height;

		Invalidate();
	}
}
