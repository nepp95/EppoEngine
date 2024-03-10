#include "pch.h"
#include "Shader.h"

#include "Core/Hash.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/RendererAPI.h"

#include <shaderc/shaderc.hpp>

namespace Eppo
{
	Shader::Shader(const ShaderSpecification& specification)
		: m_Specification(specification)
	{
		m_Name = m_Specification.Filepath.stem().string();
	}

	Ref<Shader> Shader::Create(const ShaderSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLShader>::Create(specification).As<Shader>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanShader>::Create(specification).As<Shader>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

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
			EPPO_ASSERT(eol != std::string::npos);

			// Extract shader stage
			const size_t begin = pos + stageTokenLength + 1;
			const std::string stage = std::string(source.substr(begin, eol - begin));
			EPPO_ASSERT((bool)Utils::StringToShaderStage(stage));

			// If there is no other stage token, take the string till eof. Otherwise till the next stage token
			const size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			EPPO_ASSERT(nextLinePos != std::string::npos);
			pos = source.find(stageToken, nextLinePos);
			shaderSources[Utils::StringToShaderStage(stage)] = (pos == std::string::npos) ? std::string(source.substr(nextLinePos)) : std::string(source.substr(nextLinePos, pos - nextLinePos));
		}

		return shaderSources;
	}

	void Shader::Compile(ShaderStage stage, const std::string& source)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;

		if (RendererAPI::Current() == RendererAPIType::OpenGL)
			options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
		else
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

		if (m_Specification.Optimize)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		else
			options.SetOptimizationLevel(shaderc_optimization_level_zero);

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
			}
			else
			{
				EPPO_WARN("Triggering recompilation of shader due to hash mismatch: {}.glsl (Stage: {})", m_Name, Utils::ShaderStageToString(stage));

				Compile(stage, source);
			}
		}
	}
}
