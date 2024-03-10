#include "pch.h"
#include "VulkanShader.h"

#include "Core/Filesystem.h"
#include "Core/Hash.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanRenderer.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Eppo
{
	namespace Utils
	{
		inline shaderc_shader_kind ShaderStageToShaderCKind(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return shaderc_vertex_shader;
				case ShaderStage::Fragment:	return shaderc_fragment_shader;
			}

			EPPO_ASSERT(false);
			return (shaderc_shader_kind)-1;
		}
	}

	VulkanShader::VulkanShader(const ShaderSpecification& specification)
		: Shader(specification)
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::VulkanShader");

		// Read source
		const std::string shaderSource = Filesystem::ReadText(m_Specification.Filepath);

		// Preprocess shader
		std::unordered_map<ShaderStage, std::string> sources = PreProcess(shaderSource);

		// Compile or get cached shader
		CompileOrGetCache(sources);

		// Reflect
		Reflect();

		CreatePipelineShaderInfos();
		CreateDescriptorSetLayout();
	}

	VulkanShader::~VulkanShader()
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::~VulkanShader");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (auto& shaderInfo : m_ShaderInfos)
			vkDestroyShaderModule(device, shaderInfo.module, nullptr);
	}

	ShaderDescriptorSet VulkanShader::AllocateDescriptorSet(uint32_t set)
	{
		return {};
	}

	const VkDescriptorSetLayout& VulkanShader::GetDescriptorSetLayout(uint32_t set) const
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::GetDescriptorSetLayout");
		EPPO_ASSERT((set < m_DescriptorSetLayouts.size()));

		return m_DescriptorSetLayouts.at(set);
	}

	void VulkanShader::Reflect()
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::Reflect");

		m_ShaderResources[0] = {};
		m_ShaderResources[1] = {};
		m_ShaderResources[2] = {};
		m_ShaderResources[3] = {};

		for (auto&& [stage, data] : m_ShaderBytes)
		{
			spirv_cross::Compiler compiler(data);
			spirv_cross::ShaderResources resources = compiler.get_shader_resources();

			EPPO_TRACE("Shader::Reflect - {}.glsl (Stage: {})", m_Name, Utils::ShaderStageToString(stage));
			EPPO_TRACE("    {} Push constant buffers", resources.push_constant_buffers.size());
			EPPO_TRACE("    {} Uniform buffers", resources.uniform_buffers.size());
			EPPO_TRACE("    {} Sampled images", resources.sampled_images.size());

			if (!resources.push_constant_buffers.empty())
			{
				EPPO_TRACE("    Push constant buffers:");
				EPPO_ASSERT(resources.push_constant_buffers.size() == 1); // At the moment, vulkan only supports one push constant buffer

				const auto& resource = resources.push_constant_buffers[0];
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
				size_t memberCount = bufferType.member_types.size();

				if (!resource.name.empty())
					EPPO_TRACE("        {}", resource.name);
				EPPO_TRACE("        Size = {}", bufferSize);
				EPPO_TRACE("        Members = {}", memberCount);

				for (size_t i = 0; i < memberCount; i++)
					EPPO_TRACE("            Member: {} ({})", compiler.get_member_name(resource.base_type_id, i), compiler.get_type(resource.base_type_id).member_types[i]);
			}

			if (!resources.uniform_buffers.empty())
			{
				EPPO_TRACE("    Uniform buffers:");

				for (const auto& resource : resources.uniform_buffers)
				{
					const auto& bufferType = compiler.get_type(resource.base_type_id);
					uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);

					uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
					uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
					size_t memberCount = bufferType.member_types.size();

					ShaderResource shaderResource;
					shaderResource.Type = stage;
					shaderResource.ResourceType = ShaderResourceType::UniformBuffer;
					shaderResource.Binding = binding;
					shaderResource.Size = bufferSize;
					shaderResource.Name = resource.name;

					m_ShaderResources[set].push_back(shaderResource);

					EPPO_TRACE("        {}", resource.name);
					EPPO_TRACE("            Size = {}", bufferSize);
					EPPO_TRACE("            Set = {}", set);
					EPPO_TRACE("            Binding = {}", binding);
					EPPO_TRACE("            Members = {}", memberCount);
				}
			}

			if (!resources.sampled_images.empty())
			{
				EPPO_TRACE("    Sampled images:");

				for (const auto& resource : resources.sampled_images)
				{
					const auto& bufferType = compiler.get_type(resource.base_type_id);

					uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
					uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
					size_t memberCount = bufferType.member_types.size();
					auto& spirVtype = compiler.get_type(resource.type_id);
					uint32_t arraySize = spirVtype.array[0];

					ShaderResource shaderResource;
					shaderResource.Type = stage;
					shaderResource.ResourceType = ShaderResourceType::Sampler;
					shaderResource.Binding = binding;
					shaderResource.ArraySize = arraySize;
					shaderResource.Name = resource.name;

					m_ShaderResources[set].push_back(shaderResource);

					EPPO_TRACE("        Set = {}", set);
					EPPO_TRACE("        Binding = {}", binding);
				}
			}
			EPPO_TRACE("");
		}
	}

	void VulkanShader::CreatePipelineShaderInfos()
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::CreatePipelineShaderInfos");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (const auto& [type, shaderBytes] : m_ShaderBytes)
		{
			VkShaderModuleCreateInfo shaderModuleInfo{};
			shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleInfo.codeSize = shaderBytes.size() * sizeof(uint32_t);
			shaderModuleInfo.pCode = shaderBytes.data();

			VkShaderModule shaderModule;
			VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule), "Failed to create shader module!");

			VkPipelineShaderStageCreateInfo shaderStageInfo{};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = Utils::ShaderStageToVkShaderStage(type);
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.pName = "main";

			m_ShaderInfos.push_back(shaderStageInfo);
		}
	}

	void VulkanShader::CreateDescriptorSetLayout()
	{
		EPPO_PROFILE_FUNCTION("VulkanShader::CreateDescriptorSetLayout");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		Ref<DescriptorLayoutCache> layoutCache = VulkanRenderer::GetDescriptorLayoutCache();

		for (const auto& [set, setResources] : m_ShaderResources)
		{
			if (set >= m_DescriptorSetLayouts.size())
				m_DescriptorSetLayouts.resize(set + 1);

			m_DescriptorSetLayouts[set] = layoutCache->CreateLayout(setResources);
		}

		for (uint32_t i = 0; i < m_DescriptorSetLayouts.size(); i++)
		{
			if (!m_DescriptorSetLayouts[i])
			{
				VkDescriptorSetLayoutCreateInfo layoutInfo{};
				layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutInfo.bindingCount = 0;
				layoutInfo.pBindings = nullptr;

				m_DescriptorSetLayouts[i] = layoutCache->CreateLayout(layoutInfo);
			}
		}
	}
}
