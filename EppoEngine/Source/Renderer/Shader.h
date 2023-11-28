#pragma once

#include "Renderer/Descriptor/DescriptorLayoutCache.h"
#include "Renderer/Vulkan.h"

namespace Eppo
{
	// For compatibility with all devices, we only use 4 different sets
	// 
	// Set 0 = Per frame global data
	// Set 1 = Per frame render pass data
	// Set 2 = Per frame material data
	// Set 3 = Per frame object data

	enum class ShaderStage
	{
		None = 0,
		Vertex,
		Fragment
	};

	enum class ShaderResourceType
	{
		Sampler,
		UniformBuffer
	};

	struct ShaderResource
	{
		ShaderStage Type;
		ShaderResourceType ResourceType;
		uint32_t Binding = 0;
		uint32_t Size = 0;
		uint32_t ArraySize = 0;
		std::string Name;
	};

	struct ShaderSpecification
	{
		std::filesystem::path Filepath;
		bool Optimize = false;
	};

	struct ShaderDescriptorSet
	{
		VkDescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> DescriptorSets;
	};

	class Shader
	{
	public:
		Shader(const ShaderSpecification& specification);
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		~Shader();

		ShaderDescriptorSet AllocateDescriptorSet(uint32_t set);

		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageInfos() const { return m_ShaderInfos; }
		const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
		const VkDescriptorSetLayout& GetDescriptorSetLayout(uint32_t set) const;
		const std::unordered_map<uint32_t, std::vector<ShaderResource>>& GetShaderResources() const { return m_ShaderResources; }

		const std::string& GetName() const { return m_Name; }

	private:
		std::unordered_map<ShaderStage, std::string> PreProcess(std::string_view source);
		void Compile(ShaderStage stage, const std::string& source);
		void CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources);

		void Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes);
		void CreatePipelineShaderInfos();
		void CreateDescriptorSetLayout();

	private:
		ShaderSpecification m_Specification;
		std::string m_Name;

		std::unordered_map<ShaderStage, std::vector<uint32_t>> m_ShaderBytes;
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
