#pragma once

#include "Platform/Vulkan/Descriptor/DescriptorLayoutCache.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	struct ShaderDescriptorSet
	{
		VkDescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> DescriptorSets;
	};

	class VulkanShader : public Shader
	{
	public:
		VulkanShader(const ShaderSpecification& specification);
		~VulkanShader();

		ShaderDescriptorSet AllocateDescriptorSet(uint32_t set);

		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageInfos() const { return m_ShaderInfos; }
		const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
		const VkDescriptorSetLayout& GetDescriptorSetLayout(uint32_t set) const;
		const std::unordered_map<uint32_t, std::vector<ShaderResource>>& GetShaderResources() const { return m_ShaderResources; }

	protected:
		void Reflect() override;

	private:
		void CreatePipelineShaderInfos();
		void CreateDescriptorSetLayout();

	private:
		ShaderSpecification m_Specification;
		std::string m_Name;

		std::unordered_map<uint32_t, std::vector<ShaderResource>> m_ShaderResources;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderInfos;
		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
	};

	namespace Utils
	{
		static VkDescriptorType ShaderResourceTypeToVkDescriptorType(ShaderResourceType type)
		{
			switch (type)
			{
				case ShaderResourceType::Sampler:		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				case ShaderResourceType::UniformBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			}

			EPPO_ASSERT(false);
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}

		static VkShaderStageFlagBits ShaderStageToVkShaderStage(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return VK_SHADER_STAGE_VERTEX_BIT;
				case ShaderStage::Fragment:	return VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			EPPO_ASSERT(false);
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}
	}
}
