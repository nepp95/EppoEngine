#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
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
		std::string Name;
	};

	struct ShaderSpecification
	{
		std::unordered_map<ShaderType, std::filesystem::path> ShaderSources;
	};

	class Shader
	{
	public:
		Shader(const ShaderSpecification& specification);
		~Shader();

		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageInfos() const { return m_ShaderInfos; }

	private:
		void Compile(ShaderType type, const std::filesystem::path& filepath);
		void Reflect(ShaderType type, const std::vector<uint32_t>& shaderBytes);
		void CreatePipelineShaderInfos();
		void CreateDescriptorSetLayout();

	private:
		ShaderSpecification m_Specification;

		std::unordered_map<ShaderType, std::vector<uint32_t>> m_ShaderBytes;
		std::unordered_map<uint32_t, std::vector<ShaderResource>> m_ShaderResources;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderInfos;
		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
	};
}
