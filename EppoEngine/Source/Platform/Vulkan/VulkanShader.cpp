#include "pch.h"
#include "VulkanShader.h"

#include "Core/Filesystem.h"
#include "Core/Hash.h"
#include "Platform/Vulkan/DescriptorLayoutBuilder.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/ShaderIncluder.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>

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

		inline VkShaderStageFlagBits ShaderStageToVkShaderStage(ShaderStage stage)
		{
			if (stage == ShaderStage::Vertex)       return VK_SHADER_STAGE_VERTEX_BIT;
			if (stage == ShaderStage::Fragment)     return VK_SHADER_STAGE_FRAGMENT_BIT;

			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}

		inline VkDescriptorType ShaderResourceTypeToVkDescriptorType(ShaderResourceType type)
		{
			if (type == ShaderResourceType::UniformBuffer)      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			if (type == ShaderResourceType::Sampler)            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	}

	VulkanShader::VulkanShader(const ShaderSpecification& specification)
		: Shader(specification)
	{
		// Read shader source
		const std::string shaderSource = Filesystem::ReadText(GetSpecification().Filepath);

		// Preprocess by shader stage
		auto sources = PreProcess(shaderSource);

		// Compile or get cache
		CompileOrGetCache(sources);

		m_ShaderResources[0] = {};
		m_ShaderResources[1] = {};
		m_ShaderResources[2] = {};
		m_ShaderResources[3] = {};

		// Reflection
		for (const auto& [type, data] : m_ShaderBytes)
			Reflect(type, data);

		CreatePipelineShaderInfos();
		CreateDescriptorSetLayouts();
	}

	std::unordered_map<Eppo::ShaderStage, std::string> VulkanShader::PreProcess(std::string_view source)
	{
		std::unordered_map<ShaderStage, std::string> shaderSources;

		// Find stage token
		constexpr char* stageToken = "#stage";
		const size_t stageTokenLength = strlen(stageToken);
		size_t pos = source.find(stageToken, 0);

		// Process entire source
		while (pos != std::string::npos)
		{
			// Make sure we aren't eol after type token
			const size_t eol = source.find_first_of("\r\n", pos);
			EPPO_ASSERT(eol != std::string::npos); // "Syntax error: No stage specified!"

			// Extract shader stage
			const size_t begin = pos + stageTokenLength + 1;
			const auto stage = std::string(source.substr(begin, eol - begin));
			EPPO_ASSERT((bool)Utils::StringToShaderStage(stage)); // "Invalid stage specified!"

			// If there is no other stage token, take the string till eof. Otherwise till the next stage token
			const size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			EPPO_ASSERT(nextLinePos != std::string::npos); // "Syntax error: No source after stage token!"
			pos = source.find(stageToken, nextLinePos);
			shaderSources[Utils::StringToShaderStage(stage)] = (pos == std::string::npos) ? std::string(source.substr(nextLinePos)) : std::string(source.substr(nextLinePos, pos - nextLinePos));
		}

		return shaderSources;
	}

	void VulkanShader::Compile(ShaderStage stage, const std::string& source)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		options.SetIncluder(CreateScope<ShaderIncluder>());
		options.SetOptimizationLevel(shaderc_optimization_level_zero);

		// Compile source
		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ShaderStageToShaderCKind(stage), GetSpecification().Filepath.string().c_str(), options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			EPPO_ERROR("Failed to compile shader with filename: {}", GetSpecification().Filepath);
			EPPO_ERROR(result.GetErrorMessage());
			EPPO_ASSERT(false);
		}

		m_ShaderBytes[stage] = std::vector(result.cbegin(), result.cend());

		// TODO:
		// Write cache
		std::string cachePath = Utils::GetOrCreateCacheDirectory().string() + "/" + GetName() + "." + Utils::ShaderStageToString(stage);
		Filesystem::WriteBytes(cachePath, m_ShaderBytes.at(stage));

		// Write cache hash
		std::string cacheHashPath = cachePath + ".hash";
		uint64_t hash = Hash::GenerateFnv(source);
		Filesystem::WriteText(cacheHashPath, std::to_string(hash));
	}

	void VulkanShader::CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources)
	{
		const std::filesystem::path cacheDir = Utils::GetOrCreateCacheDirectory();

		for (const auto& [stage, source] : sources)
		{
			std::string cacheFile = cacheDir.string() + "/" + GetName() + "." + Utils::ShaderStageToString(stage);
			std::string cacheHashFile = cacheFile + ".hash";

			// Check if cache needs to be busted
			bool cacheVerified = false;
			if (Filesystem::Exists(cacheFile) && Filesystem::Exists(cacheHashFile))
			{
				std::string hash = std::to_string(Hash::GenerateFnv(source));
				std::string cacheHash = Filesystem::ReadText(cacheHashFile);

				// Check if cache needs to be busted
				if (cacheHash == hash)
					cacheVerified = true;
			}

			if (cacheVerified)
			{
				EPPO_INFO("Loading shader cache: {}.glsl (Stage: {})", GetName(), Utils::ShaderStageToString(stage));

				// Read shader cache
				ScopedBuffer buffer = Filesystem::ReadBytes(cacheFile);

				// Since the buffer size is 1 byte aligned and a uint32_t is 4 byte aligned, we only need a quarter of the size
				std::vector<uint32_t> vec(buffer.Size() / sizeof(uint32_t));

				// Copy the data into the vector
				memcpy(vec.data(), buffer.Data(), buffer.Size());
				m_ShaderBytes[stage] = vec;
			}
			else
			{
				EPPO_INFO("Triggering recompilation of shader due to hash mismatch: {}.glsl (Stage: {})", GetName(), Utils::ShaderStageToString(stage));

				Compile(stage, source);
			}
		}
	}

	void VulkanShader::Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes)
	{
		spirv_cross::Compiler compiler(shaderBytes);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		EPPO_TRACE("Shader::Reflect - {}.glsl (Stage: {})", GetName(), Utils::ShaderStageToString(stage));
		EPPO_TRACE("    {} Push constants", resources.push_constant_buffers.size());
		EPPO_TRACE("    {} Uniform buffers", resources.uniform_buffers.size());
		EPPO_TRACE("    {} Sampled images", resources.sampled_images.size());

		if (!resources.push_constant_buffers.empty())
		{
			EPPO_TRACE("    Push constants:");
			EPPO_ASSERT(resources.push_constant_buffers.size() == 1); // At the moment, vulkan only supports one push constant buffer

			const auto& resource = resources.push_constant_buffers[0];
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			size_t bufferSize = compiler.get_declared_struct_size(bufferType);
			size_t memberCount = bufferType.member_types.size();

			if (m_PushConstantRanges.empty())
			{
				VkPushConstantRange& pcr = m_PushConstantRanges.emplace_back();
				pcr.size = bufferSize;
				pcr.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
				pcr.offset = 0;
			}

			if (!resource.name.empty())
				EPPO_TRACE("        {}", resource.name);
			EPPO_TRACE("        Size = {}", bufferSize);
			EPPO_TRACE("        Members = {}", memberCount);

			for (size_t i = 0; i < memberCount; i++)
				EPPO_TRACE("            Member: {} ({})",
					compiler.get_member_name(resource.base_type_id, static_cast<uint32_t>(i)),
					static_cast<uint32_t>(compiler.get_type(resource.base_type_id).member_types[i]));
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

				bool bindingExists = false;
				for (auto& sr : m_ShaderResources[set])
				{
					if (sr.Binding == binding)
					{
						sr.Type = ShaderStage::All;
						bindingExists = true;
						break;
					}
				}

				if (!bindingExists)
				{
					ShaderResource& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Type = stage;
					shaderResource.ResourceType = ShaderResourceType::UniformBuffer;
					shaderResource.Binding = binding;
					shaderResource.Size = bufferSize;
					shaderResource.Name = resource.name;
				}

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
				uint32_t arraySize = 0;
				if (!spirVtype.array.empty())
					arraySize = spirVtype.array[0];

				bool bindingExists = false;
				for (auto& sr : m_ShaderResources[set])
				{
					if (sr.Binding == binding)
					{
						sr.Type = ShaderStage::All;
						bindingExists = true;
						break;
					}
				}

				if (!bindingExists)
				{
					ShaderResource& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Type = stage;
					shaderResource.ResourceType = ShaderResourceType::Sampler;
					shaderResource.Binding = binding;
					shaderResource.ArraySize = arraySize;
					shaderResource.Name = resource.name;
				}

				EPPO_TRACE("        Set = {}", set);
				EPPO_TRACE("        Binding = {}", binding);
			}
		}
		EPPO_TRACE("");
	}

	void VulkanShader::CreatePipelineShaderInfos()
	{
		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (const auto& [type, shaderBytes] : m_ShaderBytes)
		{
			VkShaderModuleCreateInfo shaderModuleCreateInfo{};
			shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCreateInfo.codeSize = shaderBytes.size() * sizeof(uint32_t);
			shaderModuleCreateInfo.pCode = shaderBytes.data();

			VkShaderModule shaderModule;
			VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule), "Failed to create shader module!");

			VkPipelineShaderStageCreateInfo& shaderStageCreateInfo = m_ShaderInfos.emplace_back();
			shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageCreateInfo.stage = Utils::ShaderStageToVkShaderStage(type);
			shaderStageCreateInfo.module = shaderModule;
			shaderStageCreateInfo.pName = "main";

			context->SubmitResourceFree([=]()
			{
				EPPO_WARN("Releasing shader module {}", (void*)shaderModule);
				vkDestroyShaderModule(device, shaderModule, nullptr);
			}, false);
		}
	}

	void VulkanShader::CreateDescriptorSetLayouts()
	{
		VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();

		m_DescriptorSetLayouts.resize(4);
		for (const auto& [set, setResources] : m_ShaderResources)
		{
			if (setResources.empty())
				continue;

			DescriptorLayoutBuilder builder;

			for (const auto& resource : setResources)
			{
				// TODO: This is as dirty as code can get
				if (resource.Binding == 0 && set == 1)
				{
					builder.AddBinding(resource.Binding, Utils::ShaderResourceTypeToVkDescriptorType(resource.ResourceType), 512);
					continue;
				}

				builder.AddBinding(resource.Binding, Utils::ShaderResourceTypeToVkDescriptorType(resource.ResourceType), resource.ArraySize);
			}

			m_DescriptorSetLayouts[set] = builder.Build(VK_SHADER_STAGE_ALL);
		}

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = 0;
		createInfo.pBindings = nullptr;

		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout), "Failed to create descriptor set layout!");

		for (auto& descriptorSetLayout : m_DescriptorSetLayouts)
		{
			if (!descriptorSetLayout)
				descriptorSetLayout = layout;
		}

		Ref<VulkanContext> context = VulkanContext::Get();
		context->SubmitResourceFree([this]()
		{
			VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();

			// Since we ALWAYS have 4 descriptor set layouts per shader AND some of them can be the same if not all descriptor sets are in use
			// We keep track of which we have freed so we don't free the same layout twice.
			std::unordered_set<void*> layoutsFreed;

			for (auto& descriptorSetLayout : m_DescriptorSetLayouts)
			{
				if (layoutsFreed.find((void*)descriptorSetLayout) == layoutsFreed.end())
				{
					layoutsFreed.insert((void*)descriptorSetLayout);
					EPPO_WARN("Releasing descriptor set layout {}", (void*)descriptorSetLayout);
					vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				}
			}
		});
	}
}
