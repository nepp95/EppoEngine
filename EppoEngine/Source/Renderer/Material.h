#pragma once

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

namespace Eppo
{
	class Material
	{
	public:
		Material(Ref<Shader> shader);
		~Material();

		void Set(const std::string& name, const Ref<Texture>& texture, uint32_t arrayIndex = 0);
		VkDescriptorSet Get();

	private:
		Ref<Shader> m_Shader;
		Ref<Texture> m_Texture; // TODO: Can be multiple in case of meshes?

		uint32_t m_Binding;

		std::vector<VkDescriptorSet> m_DescriptorSets;
	};
}
