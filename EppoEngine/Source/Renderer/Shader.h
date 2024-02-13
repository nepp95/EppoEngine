#pragma once

#include "Core/Filesystem.h"

namespace Eppo
{
	// For compatibility with all devices, we only use 4 different sets
	// 
	// Set 0 = Per frame global data
	// Set 1 = Per frame render pass data
	// Set 2 = Per frame material data
	// Set 3 = Per frame object data

	enum class ShaderStage
	{
		None = 0,
		Vertex,
		Fragment
	};

	enum class ShaderResourceType
	{
		Sampler,
		UniformBuffer
	};

	struct ShaderResource
	{
		ShaderStage Type;
		ShaderResourceType ResourceType;
		uint32_t Binding = 0;
		uint32_t Size = 0;
		uint32_t ArraySize = 0;
		std::string Name;
	};

	struct ShaderSpecification
	{
		std::filesystem::path Filepath;
		bool Optimize = false;
	};

	class Shader : public RefCounter
	{
	public:
		virtual ~Shader() {};

		const ShaderSpecification& GetSpecification() const { return m_Specification; }
		const std::string& GetName() const { return m_Name; }

		static Ref<Shader> Create(const ShaderSpecification& specification);

	protected:
		Shader(const ShaderSpecification& specification);

		virtual void Reflect() = 0;

		std::unordered_map<ShaderStage, std::string> PreProcess(std::string_view source);
		void Compile(ShaderStage stage, const std::string& source);
		void CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources);
	
	protected:
		ShaderSpecification m_Specification;
		std::string m_Name;

		std::unordered_map<ShaderStage, std::vector<uint32_t>> m_ShaderBytes;
	};

	namespace Utils
	{
		inline std::filesystem::path GetCacheDirectory()
		{
			if (!Filesystem::Exists("Resources/Shaders/Cache"))
				std::filesystem::create_directories("Resources/Shaders/Cache");

			return "Resources/Shaders/Cache";
		}

		inline ShaderStage StringToShaderStage(std::string_view stage)
		{
			if (stage == "vert")			return ShaderStage::Vertex;
			if (stage == "frag")			return ShaderStage::Fragment;

			EPPO_ASSERT(false);
			return ShaderStage::None;
		}

		inline std::string ShaderStageToString(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return "vert";
				case ShaderStage::Fragment:	return "frag";
			}

			EPPO_ASSERT(false);
			return "Invalid";
		}

	}
}
