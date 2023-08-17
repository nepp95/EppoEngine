#pragma once

#include "Renderer/Descriptors/DescriptorLayoutInfo.h"

namespace Eppo
{
	struct ShaderResource;

	// Descriptor abstraction from https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
	class DescriptorLayoutCache
	{
	public:
		DescriptorLayoutCache() = default;
		~DescriptorLayoutCache() = default;

		void Shutdown();

		VkDescriptorSetLayout CreateLayout(const VkDescriptorSetLayoutCreateInfo& createInfo);
		VkDescriptorSetLayout CreateLayout(const std::vector<ShaderResource>& shaderResources);

	private:
		std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_LayoutCache;
	};
}
