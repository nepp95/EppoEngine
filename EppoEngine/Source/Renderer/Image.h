#pragma once

#include "Renderer/Allocator.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	enum class ImageFormat
	{
		None = 0,

		// Color
		RGBA8,

		// Depth
		Depth
	};

	enum class ImageUsage
	{
		Texture,
		Attachment
	};

	struct ImageSpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;

		ImageFormat Format;
		ImageUsage Usage;

		std::filesystem::path Filepath;

		bool CubeMap = false;

		ImageSpecification() = default;
		ImageSpecification(const std::filesystem::path& filepath)
			: Filepath(filepath)
		{}
	};

	struct ImageInfo
	{
		VkImage Image;
		VkImageView ImageView;
		VkImageLayout ImageLayout;
		VkSampler Sampler;
		VmaAllocation Allocation;
	};

	class Image
	{
	public:
		Image() = default;
		Image(const ImageSpecification& specification);
		~Image();

		void Release();

		const ImageSpecification& GetSpecification() const { return m_Specification; }

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

		ImageInfo& GetImageInfo() { return m_ImageInfo; }

		static void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout);
		static VkImageAspectFlags GetImageAspectFlags(VkImageLayout layout);

	private:
		void SetData(void* data);

	private:
		ImageSpecification m_Specification;
		ImageInfo m_ImageInfo;

		void* m_ImageData = nullptr;
	};

	namespace Utils
	{
		inline VkFormat FindSupportedDepthFormat()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<PhysicalDevice> physicalDevice = context->GetPhysicalDevice();

			std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT };

			for (const auto& format : depthFormats)
			{
				VkFormatProperties properties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice->GetNativeDevice(), format, &properties);
				if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return format;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::None:     return VK_FORMAT_UNDEFINED;
				case ImageFormat::RGBA8:    return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::Depth:    return FindSupportedDepthFormat();
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline bool IsDepthFormat(ImageFormat format)
		{
			return format == ImageFormat::Depth;
		}
	}
}
