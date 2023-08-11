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

		const auto& resources = m_Shader->GetShaderResources();

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> descriptorTypes;
		for (const auto& resource : resources.at(SET))
		{
			VkDescriptorPoolSize& poolSize = descriptorTypes[SET].emplace_back();
			uint32_t descriptorCount = 0;
				descriptorCount += resource.ArraySize;
			poolSize.descriptorCount = descriptorCount * VulkanConfig::MaxFramesInFlight;
			poolSize.type = Utils::ShaderResourceTypeToVkDescriptorType(resource.ResourceType);
		}

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.maxSets = VulkanConfig::MaxFramesInFlight;
		poolCreateInfo.poolSizeCount = descriptorTypes.at(SET).size();
		poolCreateInfo.pPoolSizes = descriptorTypes.at(SET).data();
		poolCreateInfo.pNext = nullptr;

		VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &m_DescriptorPool), "Failed to create descriptor pool!");

		m_DescriptorSets.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &shader->GetDescriptorSetLayout(SET);
			allocInfo.pNext = nullptr;

			VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[i]), "Failed to allocate descriptor set!");
		}
	}

	Material::~Material()
	{
		EPPO_PROFILE_FUNCTION("Material::~Material");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	}

	void Material::Set(const std::string& name, const Ref<Texture>& texture, uint32_t arrayIndex)
	{
		EPPO_PROFILE_FUNCTION("Material::Set");

		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		Renderer::UpdateDescriptorSet(texture, m_DescriptorSets[imageIndex], arrayIndex);
	}

	VkDescriptorSet Material::Get()
	{
		EPPO_PROFILE_FUNCTION("Material::Get");

		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		return m_DescriptorSets[imageIndex];
	}
}
