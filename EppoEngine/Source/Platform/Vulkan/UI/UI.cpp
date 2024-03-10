#include "pch.h"
#include "UI.h"

#include "Platform/Vulkan/VulkanImage.h"

#include <backends/imgui_impl_vulkan_custom.h>

namespace Eppo::UI
{
	void Image(Ref<Eppo::Image> image, const ImVec2& size)
	{
		Ref<VulkanImage> vulkanImage = image.As<VulkanImage>();

		const auto& info = vulkanImage->GetImageInfo();
		const auto textureID = ImGui_ImplVulkan_AddTexture(info.Sampler, info.ImageView, info.ImageLayout);
		ImGui::Image(textureID, size);
	}
}
