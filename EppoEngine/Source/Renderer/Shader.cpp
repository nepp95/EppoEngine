#include "pch.h"
#include "Shader.h"

#include "Core/Buffer.h"
#include "Core/Filesystem.h"
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

		std::filesystem::path cacheDir = Utils::GetCacheDirectory();

		// Get filename i.e. shader
		std::filesystem::path vertexCachePath = cacheDir / specification.ShaderSources.at(ShaderType::Vertex).filename();
		// Replace filename with filename + extension = shader.vert
		vertexCachePath.replace_filename(vertexCachePath.filename().string() + vertexCachePath.extension().string());
		// Add extension to filename = shader.vert.shadercache
		vertexCachePath.replace_extension("shadercache");

		// Get filename i.e. shader
		std::filesystem::path fragmentCachePath = cacheDir / specification.ShaderSources.at(ShaderType::Fragment).filename();
		// Replace filename with filename + extension = shader.frag
		fragmentCachePath.replace_filename(fragmentCachePath.filename().string() + fragmentCachePath.extension().string());
		// Add extension to filename = shader.vert.shadercache
		fragmentCachePath.replace_extension("shadercache");

		if (std::filesystem::exists(vertexCachePath))
		{
			// Load shader byte code from cache
			ScopedBuffer buffer = Filesystem::ReadBytes(vertexCachePath);

			// Since the buffer size is 1 byte aligned and a uint32_t is 4 bytes, we only need a quarter of the size
			std::vector<uint32_t> vec(buffer.Size() / sizeof(uint32_t));

			// Copy the data into the vector
			memcpy(vec.data(), buffer.Data(), buffer.Size());
			m_ShaderBytes[ShaderType::Vertex] = vec;
		}
		else
		{
			// Compile shader from shader source
			Compile(ShaderType::Vertex, m_Specification.ShaderSources.at(ShaderType::Vertex));

			// Write cache
			Filesystem::WriteBytes(vertexCachePath, m_ShaderBytes.at(ShaderType::Vertex));
		}

		if (std::filesystem::exists(fragmentCachePath))
		{
			// Load shader byte code from cache
			ScopedBuffer buffer = Filesystem::ReadBytes(fragmentCachePath);

			// Since the buffer size is 1 byte aligned and a uint32_t is 4 bytes, we only need a quarter of the size
			std::vector<uint32_t> vec(buffer.Size() / sizeof(uint32_t));

			// Copy the data into the vector
			memcpy(vec.data(), buffer.Data(), buffer.Size());
			m_ShaderBytes[ShaderType::Fragment] = vec;
		}
		else
		{
			// Compile shader from shader source
			Compile(ShaderType::Fragment, m_Specification.ShaderSources.at(ShaderType::Fragment));

			// Write cache
			Filesystem::WriteBytes(fragmentCachePath, m_ShaderBytes.at(ShaderType::Fragment));
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

	void Shader::Compile(ShaderType type, const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("Shader::Compile");

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		options.SetOptimizationLevel(shaderc_optimization_level_performance);

		// Read shader source
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

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(shaderSource, Utils::ShaderTypeToShaderCKind(type), filepath.string().c_str(), options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			EPPO_ERROR("Failed to compile shader with path: {}", filepath.string());
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
		EPPO_TRACE("\t{} Uniform buffers", resources.uniform_buffers.size());
		EPPO_TRACE("\t{} Sampled images", resources.sampled_images.size());

		if (!resources.uniform_buffers.empty())
		{
			EPPO_TRACE("Uniform buffers:");

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

				EPPO_TRACE("\t{}", resource.name);
				EPPO_TRACE("\t\tSize = {}", bufferSize);
				EPPO_TRACE("\t\tSet = {}", set);
				EPPO_TRACE("\t\tBinding = {}", binding);
				EPPO_TRACE("\t\tMembers = {}", memberCount);
			}
		}

		if (!resources.sampled_images.empty())
		{
			EPPO_TRACE("Sampled images:");

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

				EPPO_TRACE("\t\tSet = {}", set);
				EPPO_TRACE("\t\tBinding = {}", binding);
			}
		}
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
