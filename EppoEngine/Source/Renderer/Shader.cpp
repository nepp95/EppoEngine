#include "pch.h"
#include "Shader.h"

#include "Core/Buffer.h"
#include "Core/Filesystem.h"
#include "Core/Hash.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Eppo
{
	namespace Utils
	{
		static std::filesystem::path GetCacheDirectory()
		{
			if (!Filesystem::Exists("Resources/Shaders/Cache"))
				std::filesystem::create_directories("Resources/Shaders/Cache");

			return "Resources/Shaders/Cache";
		}

		static shaderc_shader_kind ShaderStageToShaderCKind(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return shaderc_vertex_shader;
				case ShaderStage::Fragment:	return shaderc_fragment_shader;
			}

			EPPO_ASSERT(false);
			return (shaderc_shader_kind)-1;
		}

		static std::string ShaderStageToString(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return "vert";
				case ShaderStage::Fragment:	return "frag";
			}

			EPPO_ASSERT(false);
			return "Invalid";
		}

		static ShaderStage StringToShaderStage(std::string_view stage)
		{
			if (stage == "vert")			return ShaderStage::Vertex;
			if (stage == "frag")			return ShaderStage::Fragment;

			EPPO_ASSERT(false);
			return ShaderStage::None;
		}
	}

	Shader::Shader(const ShaderSpecification& specification)
		: m_Specification(specification), m_Name(m_Specification.Filepath.stem().string())
	{
		EPPO_PROFILE_FUNCTION("Shader::Shader");

		const std::string shaderSource = Filesystem::ReadText(m_Specification.Filepath);

		// Preprocess shader
		std::unordered_map<ShaderStage, std::string> sources = PreProcess(shaderSource);

		// Compile or get cached shader
		CompileOrGetCache(sources);

		m_ShaderResources[0] = {};
		m_ShaderResources[1] = {};
		m_ShaderResources[2] = {};
		m_ShaderResources[3] = {};

		for (auto&& [type, data] : m_ShaderBytes)
			Reflect(type, data);

		CreatePipelineShaderInfos();
		CreateDescriptorSetLayout();
	}

	Shader::~Shader()
	{
		EPPO_PROFILE_FUNCTION("Shader::~Shader");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		for (auto& shaderInfo : m_ShaderInfos)
			vkDestroyShaderModule(device, shaderInfo.module, nullptr);
	}

	const VkDescriptorSetLayout& Shader::GetDescriptorSetLayout(uint32_t set) const
	{
		EPPO_PROFILE_FUNCTION("Shader::GetDescriptorSetLayout");
		EPPO_ASSERT((set < m_DescriptorSetLayouts.size()));

		return m_DescriptorSetLayouts.at(set);
	}

	std::unordered_map<ShaderStage, std::string> Shader::PreProcess(std::string_view source)
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
			const std::string stage = std::string(source.substr(begin, eol - begin));
			EPPO_ASSERT((bool)Utils::StringToShaderStage(stage)); // "Invalid stage specified!"
	
			// If there is no other stage token, take the string till eof. Otherwise till the next stage token
			const size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			EPPO_ASSERT(nextLinePos != std::string::npos); // "Syntax error: No source after stage token!"
			pos = source.find(stageToken, nextLinePos);
			shaderSources[Utils::StringToShaderStage(stage)] = (pos == std::string::npos) ? std::string(source.substr(nextLinePos)) : std::string(source.substr(nextLinePos, pos - nextLinePos));
		}

		return shaderSources;
	}

	void Shader::Compile(ShaderStage stage, const std::string& source)
	{
		EPPO_PROFILE_FUNCTION("Shader::Compile");

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		options.SetOptimizationLevel(shaderc_optimization_level_zero); // TODO: ZERO OPTIMIZATION?...

		// Compile source
		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ShaderStageToShaderCKind(stage), m_Specification.Filepath.string().c_str());
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			EPPO_ERROR("Failed to compile shader with filename: {}", m_Specification.Filepath.string());
			EPPO_ERROR(result.GetErrorMessage());
			EPPO_ASSERT(false);
		}

		m_ShaderBytes[stage] = std::vector(result.cbegin(), result.cend());

		// Write cache
		std::string cachePath = Utils::GetCacheDirectory().string() + "/" + m_Name + "." + Utils::ShaderStageToString(stage);
		Filesystem::WriteBytes(cachePath, m_ShaderBytes.at(stage));

		// Write cache hash
		std::string cacheHashPath = cachePath + ".hash";
		uint64_t hash = Hash::GenerateFnv(source);
		Filesystem::WriteText(cacheHashPath, std::to_string(hash));
	}

	void Shader::CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources)
	{
		EPPO_PROFILE_FUNCTION("Shader::CompileOrGetCache");

		const std::filesystem::path cacheDir = Utils::GetCacheDirectory();

		for (const auto& [stage, source] : sources)
		{
			std::string cacheFile = cacheDir.string() + "/" + m_Name + "." + Utils::ShaderStageToString(stage);
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
				EPPO_INFO("Loading shader cache: {}.glsl (Stage: {})", m_Name, Utils::ShaderStageToString(stage));

				// Read shader cache
				ScopedBuffer buffer = Filesystem::ReadBytes(cacheFile);

				// Since the buffer size is 1 byte aligned and a uint32_t is 4 byte aligned, we only need a quarter of the size
				std::vector<uint32_t> vec(buffer.Size() / sizeof(uint32_t));

				// Copy the data into the vector
				memcpy(vec.data(), buffer.Data(), buffer.Size());
				m_ShaderBytes[stage] = vec;
			} else
			{
				EPPO_INFO("Triggering recompilation of shader due to hash mismatch: {}.glsl (Stage: {})", m_Name, Utils::ShaderStageToString(stage));

				Compile(stage, source);
			}
		}
	}

	void Shader::Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes)
	{
		EPPO_PROFILE_FUNCTION("Shader::Reflect");

		spirv_cross::Compiler compiler(shaderBytes);
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

	void Shader::CreatePipelineShaderInfos()
	{
		EPPO_PROFILE_FUNCTION("Shader::CreatePipelineShaderInfos");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

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

	void Shader::CreateDescriptorSetLayout()
	{
		EPPO_PROFILE_FUNCTION("Shader::CreateDescriptorSetLayout");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		Ref<DescriptorLayoutCache> layoutCache = Renderer::GetDescriptorLayoutCache();

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
