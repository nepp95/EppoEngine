#include "pch.h"
#include "Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"
#include "Renderer/RendererAPI.h"

#include <stb_image.h>

namespace Eppo
{
	Texture::Texture(const std::filesystem::path& filepath)
		: m_Filepath(filepath)
	{
		// Read pixels
		int width, height, channels;
		stbi_uc* data = nullptr;

		m_ImageData.Data = stbi_load(m_Filepath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		m_ImageData.Size = width * height * channels;
		m_Width = width;
		m_Height = height;
	}

	Texture::Texture(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{}

	Ref<Texture> Texture::Create(const std::filesystem::path& filepath)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLTexture>::Create(filepath).As<Texture>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanTexture>::Create(filepath).As<Texture>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

	Ref<Texture> Texture::Create(uint32_t width, uint32_t height, ImageFormat format, void* data)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLTexture>::Create(width, height, format, data).As<Texture>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanTexture>::Create(width, height, format, data).As<Texture>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
