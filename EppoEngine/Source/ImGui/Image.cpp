#include "pch.h"
#include "ImGui/Image.h"

#include "Platform/Vulkan/VulkanImage.h"

#include <backends/imgui_impl_vulkan.h>

namespace Eppo::UI
{
	struct ImageInfo
	{
		VkImageLayout Layout;
		VkImageView ImageView;
		VkSampler Sampler;
		VkDescriptorSet DescriptorSet;
	};

	static std::unordered_map<void*, ImageInfo> s_ImageCache;

	void Image(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0, const ImVec2& uv1)
	{
		auto vkImage = std::dynamic_pointer_cast<VulkanImage>(image);
		if (!vkImage)
		{
			EPPO_ERROR("Trying to use non vulkan image in vulkan imgui!");
			return;
		}

		const auto& imageInfo = vkImage->GetImageInfo();

		if (s_ImageCache.find((void*)imageInfo.ImageView) == s_ImageCache.end())
		{
			ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];
			info.Layout = imageInfo.ImageLayout;
			info.ImageView = imageInfo.ImageView;
			info.Sampler = imageInfo.Sampler;
			info.DescriptorSet = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, imageInfo.ImageLayout);
		}

		ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];

		ImGui::Image((ImTextureID)info.DescriptorSet, imageSize, uv0, uv1);
	}

	bool ImageButton(const std::string& id, Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0, const ImVec2& uv1)
	{
		auto vkImage = std::dynamic_pointer_cast<VulkanImage>(image);
		if (!vkImage)
		{
			EPPO_ERROR("Trying to use non vulkan image in vulkan imgui!");
			return false;
		}

		const auto& imageInfo = vkImage->GetImageInfo();

		if (s_ImageCache.find((void*)imageInfo.ImageView) == s_ImageCache.end())
		{
			ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];
			info.Layout = imageInfo.ImageLayout;
			info.ImageView = imageInfo.ImageView;
			info.Sampler = imageInfo.Sampler;
			info.DescriptorSet = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, imageInfo.ImageLayout);
		}

		ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];

		return ImGui::ImageButton(id.c_str(), (ImTextureID)info.DescriptorSet, imageSize, uv0, uv1);
	}

	bool ImageButton(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0, const ImVec2& uv1)
	{
		return ImageButton(image->GetSpecification().Filepath.string(), image, imageSize, uv0, uv1);
	}
}
