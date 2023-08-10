#include "pch.h"
#include "Material.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Material::Material(Ref<Shader> shader)
		: m_Shader(shader)
	{
		const auto& resources = m_Shader->GetShaderResources();

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> descriptorTypes;
		for (const auto& [set, resource] : resources)
		{
			VkDescriptorPoolSize& poolSize = descriptorTypes[set].emplace_back();
			uint32_t descriptorCount = 0;
			for (const auto& innerResource : resource)
				descriptorCount += innerResource.ArraySize;
			poolSize.descriptorCount = descriptorCount * 2;
			poolSize.type = Utils::ShaderResourceTypeToVkDescriptorType(resource[0].ResourceType);
		}

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		m_DescriptorPools.resize(descriptorTypes.size());
		for (const auto& [set, types] : descriptorTypes)
		{
			VkDescriptorPoolCreateInfo poolCreateInfo{};
			poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolCreateInfo.maxSets = 2;
			poolCreateInfo.poolSizeCount = descriptorTypes.at(set).size();
			poolCreateInfo.pPoolSizes = descriptorTypes.at(set).data();
			poolCreateInfo.pNext = nullptr;

			VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &m_DescriptorPools[set]), "Failed to create descriptor pool!");

			m_DescriptorSets[set].resize(VulkanConfig::MaxFramesInFlight);
			for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			{
				VkDescriptorSetAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorPool = m_DescriptorPools[set];
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &shader->GetDescriptorSetLayout(set);
				allocInfo.pNext = nullptr;

				VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[set][i]), "Failed to allocate descriptor set!");
			}
		}
	}

	Material::~Material()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		for (uint32_t i = 0; i < m_DescriptorPools.size(); i++)
			vkDestroyDescriptorPool(device, m_DescriptorPools[i], nullptr);
	}

	void Material::Set(const std::string& name, const Ref<Texture>& texture, uint32_t arrayIndex)
	{
		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		Renderer::UpdateDescriptorSet(texture, m_DescriptorSets[0][imageIndex], arrayIndex);
	}

	VkDescriptorSet Material::Get()
	{
		return m_DescriptorSets[0][0];
	}
}
