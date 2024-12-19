#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	class VulkanShader : public Shader
	{
	public:
		explicit VulkanShader(const ShaderSpecification& specification);

		[[nodiscard]] const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageInfos() const { return m_ShaderInfos; }
		[[nodiscard]] const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
		[[nodiscard]] const std::vector<VkPushConstantRange>& GetPushConstantRanges() const { return m_PushConstantRanges; }

	private:
		[[nodiscard]] std::unordered_map<ShaderStage, std::string> PreProcess(std::string_view source) const;
		void Compile(ShaderStage stage, const std::string& source);
		void CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources);
		void Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes);
		void CreatePipelineShaderInfos();
		void CreateDescriptorSetLayouts();

	private:
		std::unordered_map<ShaderStage, std::vector<uint32_t>> m_ShaderBytes;
		std::unordered_map<uint32_t, std::vector<ShaderResource>> m_ShaderResources;

		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderInfos;
		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::vector<VkPushConstantRange> m_PushConstantRanges;
	};
}
