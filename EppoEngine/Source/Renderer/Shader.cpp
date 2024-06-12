#include "pch.h"
#include "Shader.h"

#include "Core/Filesystem.h"
#include "Core/Hash.h"
#include "Renderer/Renderer.h"

#include <glad/glad.h>
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

		static GLenum ShaderStageToGLStage(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return GL_VERTEX_SHADER;
				case ShaderStage::Fragment:	return GL_FRAGMENT_SHADER;
			}

			EPPO_ASSERT(false);
			return 0;
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

		// Read shader source
		const std::string shaderSource = Filesystem::ReadText(m_Specification.Filepath);
		 
		// Preprocess by shader stage
		std::unordered_map<ShaderStage, std::string> sources = PreProcess(shaderSource);
		
		// Compile or get cache
		CompileOrGetCache(sources);
		CreateProgram();
		
		// Reflection
		for (auto&& [type, data] : m_ShaderBytes)
			Reflect(type, data);
	}

	Shader::~Shader()
	{
		EPPO_PROFILE_FUNCTION("Shader::~Shader");
	}

	void Shader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void Shader::Unbind() const
	{
		glUseProgram(0);
	}

	std::unordered_map<ShaderStage, std::string> Shader::PreProcess(std::string_view source)
	{
		EPPO_PROFILE_FUNCTION("Shader::PreProcess");

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
		options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
		options.SetOptimizationLevel(shaderc_optimization_level_zero);

		// Compile source
		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ShaderStageToShaderCKind(stage), m_Specification.Filepath.string().c_str(), options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			EPPO_ERROR("Failed to compile shader with filename: {}", m_Specification.Filepath);
			EPPO_ERROR(result.GetErrorMessage());
			EPPO_ASSERT(false);
		}

		m_ShaderBytes[stage] = std::vector(result.cbegin(), result.cend());

		// Write cache
		//std::string cachePath = Utils::GetCacheDirectory().string() + "/" + m_Name + "." + Utils::ShaderStageToString(stage);
		//Filesystem::WriteBytes(cachePath, m_ShaderBytes.at(stage));

		// Write cache hash
		//std::string cacheHashPath = cachePath + ".hash";
		//uint64_t hash = Hash::GenerateFnv(source);
		//Filesystem::WriteText(cacheHashPath, std::to_string(hash));
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

	void Shader::CreateProgram()
	{
		EPPO_PROFILE_FUNCTION("Shader::CreateProgram");

		uint32_t program = glCreateProgram();

		std::array<uint32_t, 2> shaderIDs;
		int index = 0;
		for (auto&& [stage, spirv] : m_ShaderBytes)
		{
			spirv_cross::CompilerGLSL glslCompiler(spirv);
			auto& source = glslCompiler.compile();

			uint32_t shader = glCreateShader(Utils::ShaderStageToGLStage(stage));

			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);
			glCompileShader(shader);

			int isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE)
			{
				int maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<char> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				glDeleteShader(shader);

				EPPO_ERROR("{}", infoLog.data());
				EPPO_ASSERT(false);
			}

			glAttachShader(program, shader);
			shaderIDs[index++] = shader;
		}

		glLinkProgram(program);

		int isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<char> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
			EPPO_ERROR("Shader linking failed ({}):\n{}", m_Specification.Filepath, infoLog.data());

			glDeleteProgram(program);

			for (auto id : shaderIDs)
				glDeleteShader(id);
		}

		for (auto id : shaderIDs)
			glDetachShader(program, id);

		m_RendererID = program;
	}

	void Shader::Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes)
	{
		EPPO_PROFILE_FUNCTION("Shader::Reflect");

		spirv_cross::Compiler compiler(shaderBytes);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		EPPO_TRACE("Shader::Reflect - {}.glsl (Stage: {})", m_Name, Utils::ShaderStageToString(stage));
		EPPO_TRACE("    {} Uniform buffers", resources.uniform_buffers.size());
		EPPO_TRACE("    {} Sampled images", resources.sampled_images.size());

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
