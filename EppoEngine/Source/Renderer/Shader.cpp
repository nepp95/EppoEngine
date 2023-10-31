#include "pch.h"
#include "Shader.h"

#include "Core/Buffer.h"
#include "Core/Filesystem.h"
#include "Core/Hash.h"
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

		static shaderc_shader_kind ShaderTypeToShaderCKind(ShaderType type)
		{
			switch (type)
			{
				case ShaderType::Vertex:	return shaderc_vertex_shader;
				case ShaderType::Fragment:	return shaderc_fragment_shader;
			}

			EPPO_ASSERT(false);
			return (shaderc_shader_kind)-1;
		}

		static std::string ShaderTypeToString(ShaderType type)
		{
			switch (type)
			{
				case ShaderType::Vertex:	return "Vertex";
				case ShaderType::Fragment:	return "Fragment";
			}

			EPPO_ASSERT(false);
			return "Invalid";
		}
	}

	Shader::Shader(const ShaderSpecification& specification, const Ref<DescriptorLayoutCache>& layoutCache)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Shader::Shader");

		const std::filesystem::path cacheDir = Utils::GetCacheDirectory();

		for (const auto& [type, filepath] : m_Specification.ShaderSources)
		{
			// Calculate hash of shader source
			std::string shaderSource = ReadShaderFile(filepath);
			std::string hash = std::to_string(Hash::GenerateFnv(shaderSource));
			
			// Check if the shader is cached
			std::filesystem::path cacheFile = filepath.filename().replace_filename(filepath.filename().string() + filepath.extension().string()).replace_extension("shadercache");
			std::filesystem::path cachePath = cacheDir / cacheFile;
			std::filesystem::path cacheHashPath = cacheDir / cacheFile.replace_extension("hash");

			bool cacheVerified = false;
			if (Filesystem::Exists(cachePath))
			{
				std::string cachedHash = Filesystem::ReadText(cacheHashPath);

				if (cachedHash == hash)
					cacheVerified = true;
				else
					EPPO_INFO("Triggered recompilation of shader due to hash mismatch: {}", filepath.string());
			}

			if (cacheVerified)
			{
				// Read shader cache
				ScopedBuffer buffer = Filesystem::ReadBytes(cachePath);

				// Since the buffer size is 1 byte aligned and a uint32_t is 4 bytes, we only need a quarter of the size
				std::vector<uint32_t> vec(buffer.Size() / sizeof(uint32_t));

				// Copy the data into the vector
				memcpy(vec.data(), buffer.Data(), buffer.Size());
				m_ShaderBytes[type] = vec;
			} else 
			{
				// Compile shader
				Compile(type, shaderSource, filepath.string());

				// Write shader cache
				Filesystem::WriteBytes(cachePath, m_ShaderBytes.at(type));

				// Write shader hash
				Filesystem::WriteText(cacheHashPath, hash);
			}
		}

		m_ShaderResources[0] = {};
		m_ShaderResources[1] = {};
		m_ShaderResources[2] = {};
		m_ShaderResources[3] = {};

		for (auto&& [type, data] : m_ShaderBytes)
			Reflect(type, data);

		CreatePipelineShaderInfos();
		CreateDescriptorSetLayout(layoutCache);
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

	std::string Shader::ReadShaderFile(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::in);
		EPPO_ASSERT(stream);

		std::string shaderSource;

		stream.seekg(0, std::ios::end);
		size_t size = stream.tellg();

		if (size != -1)
		{
			shaderSource.resize(size);
			stream.seekg(0, std::ios::beg);
			stream.read(&shaderSource[0], shaderSource.size());
		}

		return shaderSource;
	}

	void Shader::Compile(ShaderType type, const std::string& shaderSource, const std::string& filename)
	{
		EPPO_PROFILE_FUNCTION("Shader::Compile");

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		options.SetOptimizationLevel(shaderc_optimization_level_zero); // TODO: ZERO OPTIMIZATION?...

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(shaderSource, Utils::ShaderTypeToShaderCKind(type), filename.c_str(), options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			EPPO_ERROR("Failed to compile shader with filename: {}", filename);
			EPPO_ERROR(result.GetErrorMessage());
			EPPO_ASSERT(false);
		}

		m_ShaderBytes.insert_or_assign(type, std::vector(result.cbegin(), result.cend()));
	}

	void Shader::Reflect(ShaderType type, const std::vector<uint32_t>& shaderBytes)
	{
		EPPO_PROFILE_FUNCTION("Shader::Reflect");

		spirv_cross::Compiler compiler(shaderBytes);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		EPPO_TRACE("Shader::Reflect - {} {}", Utils::ShaderTypeToString(type), m_Specification.ShaderSources.at(type).string());
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
				shaderResource.Type = type;
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
				shaderResource.Type = type;
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
			shaderStageInfo.stage = Utils::ShaderTypeToVkShaderStage(type);
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.pName = "main";

			m_ShaderInfos.push_back(shaderStageInfo);
		}
	}

	void Shader::CreateDescriptorSetLayout(const Ref<DescriptorLayoutCache>& layoutCache)
	{
		EPPO_PROFILE_FUNCTION("Shader::CreateDescriptorSetLayout");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

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
