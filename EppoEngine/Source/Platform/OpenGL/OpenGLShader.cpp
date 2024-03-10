#include "pch.h"
#include "OpenGLShader.h"

#include "Core/Filesystem.h"

#include <glad/glad.h>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Eppo
{
	namespace Utils
	{
		inline GLenum ShaderStageToGLenum(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return GL_VERTEX_SHADER;
				case ShaderStage::Fragment:	return GL_FRAGMENT_SHADER;
			}

			EPPO_ASSERT(false);
			return GL_INVALID_ENUM;
		}
	}

    OpenGLShader::OpenGLShader(const ShaderSpecification& specification)
        : Shader(specification)
    {
		// Read source
		const std::string shaderSource = Filesystem::ReadText(m_Specification.Filepath);

		// Preprocess shader
		std::unordered_map<ShaderStage, std::string> sources = PreProcess(shaderSource);
	
		// Compile or get cached shader
		CompileOrGetCache(sources);

		// Reflect
		Reflect();

		// Link
		Link();
	}

    OpenGLShader::~OpenGLShader()
    {
		glDeleteProgram(m_RendererID);
	}

	void OpenGLShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::Reflect()
	{
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

	void OpenGLShader::Link()
	{
		m_RendererID = glCreateProgram();

		// Link each stage to main program
		std::vector<GLuint> shaderIDs;
		for (auto&& [stage, spirv] : m_ShaderBytes)
		{
			std::vector<unsigned char> bytes(spirv.size() * sizeof(uint32_t));
			memcpy(bytes.data(), spirv.data(), spirv.size() * sizeof(uint32_t));

			GLuint shaderID = shaderIDs.emplace_back(glCreateShader(Utils::ShaderStageToGLenum(stage)));
			glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, bytes.data(), bytes.size());
			glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
			glAttachShader(m_RendererID, shaderID);
		}

		glLinkProgram(m_RendererID);

		// If linking failed
		GLint isLinked;
		glGetProgramiv(m_RendererID, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength;
			glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(m_RendererID, maxLength, &maxLength, infoLog.data());
			EPPO_ERROR("Shader linking failed for '{}': {}", m_Name, infoLog.data());

			glDeleteProgram(m_RendererID);

			for (auto id : shaderIDs)
				glDeleteShader(id);
		}

		// Cleanup
		/*for (auto id : shaderIDs)
		{
			glDetachShader(m_RendererID, id);
			glDeleteShader(id);
		}*/
	}
}
