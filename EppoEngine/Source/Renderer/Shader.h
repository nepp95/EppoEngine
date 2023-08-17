#pragma once

#include "Renderer/Descriptors/DescriptorLayoutCache.h"
#include "Renderer/Vulkan.h"

namespace Eppo
{
	// For compatibility with all devices, we only use 4 different sets
	// 
	// Set 0 = Per frame global data
	// Set 1 = Per frame render pass data
	// Set 2 = Per frame material data
	// Set 3 = Per frame object data

	enum class ShaderType
	{
		Vertex = 0,
		Fragment = 1
	};

	enum class ShaderResourceType
	{
		Sampler,
		UniformBuffer
	};

	struct ShaderResource
	{
		ShaderType Type;
		ShaderResourceType ResourceType;
		uint32_t Binding = 0;
		uint32_t Size = 0;
		uint32_t ArraySize = 0;
		std::string Name;
	};

	struct ShaderSpecification
	{
		std::unordered_map<ShaderType, std::filesystem::path> ShaderSources;
	};

	class Shader
	{
	public:
		Shader(const ShaderSpecification& specification, const Ref<DescriptorLayoutCache>& layoutCache);
		~Shader();

		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageInfos() const { return m_ShaderInfos; }
		const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
		const VkDescriptorSetLayout& GetDescriptorSetLayout(uint32_t set) const;
		const std::unordered_map<uint32_t, std::vector<ShaderResource>>& GetShaderResources() const { return m_ShaderResources; }

	private:
		void Compile(ShaderType type, const std::filesystem::path& filepath);
		void Reflect(ShaderType type, const std::vector<uint32_t>& shaderBytes);
		void CreatePipelineShaderInfos();
		void CreateDescriptorSetLayout(const Ref<DescriptorLayoutCache>& layoutCache);

	private:
		ShaderSpecification m_Specification;

		std::unordered_map<ShaderType, std::vector<uint32_t>> m_ShaderBytes;
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
		}

		static VkShaderStageFlagBits ShaderTypeToVkShaderStage(ShaderType type)
		{
			switch (type)
			{
				case ShaderType::Vertex:	return VK_SHADER_STAGE_VERTEX_BIT;
				case ShaderType::Fragment:	return VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			EPPO_ASSERT(false);
		}
	}
}
