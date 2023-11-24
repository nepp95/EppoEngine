#include "pch.h"
#include "DescriptorLayoutCache.h"

#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	void DescriptorLayoutCache::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("DescriptorLayoutCache::Shutdown");

		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (auto& layout : m_LayoutCache)
			vkDestroyDescriptorSetLayout(device, layout.second, nullptr);
	}

	VkDescriptorSetLayout DescriptorLayoutCache::CreateLayout(const VkDescriptorSetLayoutCreateInfo& createInfo)
	{
		EPPO_PROFILE_FUNCTION("DescriptorLayoutCache::CreateLayout");

		uint32_t bindingCount = createInfo.bindingCount;

		DescriptorLayoutInfo layoutInfo;
		layoutInfo.Bindings.resize(bindingCount);

		// Copy over bindings
		for (uint32_t i = 0; i < bindingCount; i++)
			layoutInfo.Bindings[i] = createInfo.pBindings[i];

		// Sort the vector
		std::sort(layoutInfo.Bindings.begin(), layoutInfo.Bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b)
		{
			return a.binding < b.binding;
		});

		// Cache hit
		auto it = m_LayoutCache.find(layoutInfo);
		if (it != m_LayoutCache.end())
			return (*it).second;

		// No cache hit
		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout), "Failed to create descriptor set layout!");

		// Add layout to cache
		m_LayoutCache[layoutInfo] = layout;

		return layout;
	}

	VkDescriptorSetLayout DescriptorLayoutCache::CreateLayout(const std::vector<ShaderResource>& shaderResources)
	{
		EPPO_PROFILE_FUNCTION("DescriptorLayoutCache::CreateLayout");

		std::vector<VkDescriptorSetLayoutBinding> bindings;

		for (const auto& resource : shaderResources)
		{
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = resource.Binding;
			binding.descriptorCount = resource.ArraySize == 0 ? 1 : resource.ArraySize;
			binding.descriptorType = Utils::ShaderResourceTypeToVkDescriptorType(resource.ResourceType);
			binding.stageFlags = Utils::ShaderStageToVkShaderStage(resource.Type);
			binding.pImmutableSamplers = nullptr;

			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings = bindings.data();

		return CreateLayout(layoutInfo);
	}
}
