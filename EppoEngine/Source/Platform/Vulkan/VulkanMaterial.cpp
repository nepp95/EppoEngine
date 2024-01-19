#include "pch.h"
#include "VulkanMaterial.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	VulkanMaterial::VulkanMaterial(Ref<Shader> shader)
		: m_Shader(shader)
	{
		EPPO_PROFILE_FUNCTION("VulkanMaterial::VulkanMaterial");

		Ref<DescriptorAllocator> allocator = VulkanRenderer::GetDescriptorAllocator();
		Ref<VulkanShader> vulkanShader = shader.As<VulkanShader>();

		m_DescriptorSets.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			allocator->Allocate(&m_DescriptorSets[i], vulkanShader->GetDescriptorSetLayout(2));
	}

	VulkanMaterial::~VulkanMaterial()
	{
		EPPO_PROFILE_FUNCTION("VulkanMaterial::~VulkanMaterial");
	}

	void VulkanMaterial::Set(const std::string& name, Ref<Texture> texture, uint32_t arrayIndex)
	{
		EPPO_PROFILE_FUNCTION("VulkanMaterial::Set");

		m_Texture = texture;

		Ref<VulkanImage> vulkanImage = m_Texture->GetImage().As<VulkanImage>();
		const auto& info = vulkanImage->GetImageInfo();

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = info.ImageLayout;
		imageInfo.imageView = info.ImageView;
		imageInfo.sampler = info.Sampler;

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

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

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
	}

	VkDescriptorSet VulkanMaterial::GetDescriptorSet(uint32_t imageIndex)
	{
		EPPO_PROFILE_FUNCTION("Material::GetDescriptorSet");
		EPPO_ASSERT((imageIndex < VulkanConfig::MaxFramesInFlight));

		return m_DescriptorSets[imageIndex];
	}

	VkDescriptorSet VulkanMaterial::GetCurrentDescriptorSet()
	{
		EPPO_PROFILE_FUNCTION("Material::GetCurrentDescriptorSet");

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

		return GetDescriptorSet(imageIndex);
	}
}
