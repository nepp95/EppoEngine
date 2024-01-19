#pragma once

#include "Renderer/Material.h"

namespace Eppo
{
	class VulkanMaterial : public Material
	{
	public:
		VulkanMaterial(Ref<Shader> shader);
		~VulkanMaterial();

		void Set(const std::string& name, Ref<Texture> texture, uint32_t arrayIndex = 0) override;

		VkDescriptorSet GetDescriptorSet(uint32_t imageIndex);
		VkDescriptorSet GetCurrentDescriptorSet();

	private:
		Ref<Shader> m_Shader;
		Ref<Texture> m_Texture; // TODO: Can be multiple in case of meshes?

		uint32_t m_Binding;

		std::vector<VkDescriptorSet> m_DescriptorSets;
	};
}
