#include "pch.h"
#include "Texture.h"

#include "Platform/Vulkan/VulkanTexture.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<Texture> Texture::Create(const std::filesystem::path& filepath)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
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
				EPPO_ASSERT(false);
				break;
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
