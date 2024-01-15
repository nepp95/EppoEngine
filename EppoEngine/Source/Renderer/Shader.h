#pragma once

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

	class Shader
	{
	public:
		virtual ~Shader() {};

		virtual const std::string& GetName() const = 0;

		static Ref<Shader> Create(const ShaderSpecification& specification);
	};
}
