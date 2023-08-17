#include "pch.h"
#include "Material.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#define SET 2

namespace Eppo
{
	Material::Material(Ref<Shader> shader)
		: m_Shader(shader)
	{
		EPPO_PROFILE_FUNCTION("Material::Material");

		Ref<DescriptorAllocator> allocator = Renderer::GetDescriptorAllocator();

		m_DescriptorSets.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			allocator->Allocate(&m_DescriptorSets[i], m_Shader->GetDescriptorSetLayout(SET));
	}

	Material::~Material()
	{
		EPPO_PROFILE_FUNCTION("Material::~Material");
	}

	void Material::Set(const std::string& name, const Ref<Texture>& texture, uint32_t arrayIndex)
	{
		EPPO_PROFILE_FUNCTION("Material::Set");
		
		m_Texture = texture;

		const auto& info = texture->GetImage()->GetImageInfo();

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = info.ImageLayout;
		imageInfo.imageView = info.ImageView;
		imageInfo.sampler = info.Sampler;

		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		VkWriteDescriptorSet writeDesc{};
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.dstSet = m_DescriptorSets[imageIndex];
		writeDesc.dstBinding = 0;
		writeDesc.dstArrayElement = arrayIndex;
		writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDesc.descriptorCount = 1;
		writeDesc.pBufferInfo = nullptr;
		writeDesc.pImageInfo = &imageInfo;
		writeDesc.pTexelBufferView = nullptr;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
	}

	VkDescriptorSet Material::Get()
	{
		EPPO_PROFILE_FUNCTION("Material::Get");

		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		return m_DescriptorSets[imageIndex];
	}
}
