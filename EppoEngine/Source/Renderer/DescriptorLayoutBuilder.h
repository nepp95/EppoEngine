#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
	class DescriptorLayoutBuilder
	{
	public:
		void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count);
		void Clear();

		VkDescriptorSetLayout Build(VkShaderStageFlags shaderStageFlags, VkDescriptorSetLayoutCreateFlags createFlags = 0, void* pNext = nullptr);

	private:
		std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
	};
}
