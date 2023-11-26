#include "pch.h"
#include "Material.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	Material::Material(Ref<Shader> shader)
		: m_Shader(shader)
	{
		EPPO_PROFILE_FUNCTION("Material::Material");

		Ref<DescriptorAllocator> allocator = Renderer::GetDescriptorAllocator();

		m_DescriptorSets.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			allocator->Allocate(&m_DescriptorSets[i], m_Shader->GetDescriptorSetLayout(2));
	}

	Material::~Material()
	{
		EPPO_PROFILE_FUNCTION("Material::~Material");
	}

	void Material::Set(const std::string& name, Ref<Texture> texture, uint32_t arrayIndex)
	{
		EPPO_PROFILE_FUNCTION("Material::Set");
		
		m_Texture = texture;

		const auto& info = m_Texture->GetImage()->GetImageInfo();

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

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
	}

	VkDescriptorSet Material::GetDescriptorSet(uint32_t imageIndex)
	{
		EPPO_PROFILE_FUNCTION("Material::GetDescriptorSet");
		EPPO_ASSERT((imageIndex < VulkanConfig::MaxFramesInFlight));

		return m_DescriptorSets[imageIndex];
	}

	VkDescriptorSet Material::GetCurrentDescriptorSet()
	{
		EPPO_PROFILE_FUNCTION("Material::GetCurrentDescriptorSet");

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

		return GetDescriptorSet(imageIndex);
	}
}
