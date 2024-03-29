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
		Shader(const ShaderSpecification& specification);
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		~Shader();

		void RT_Bind() const;
		void RT_Unbind() const;

		const std::unordered_map<uint32_t, std::vector<ShaderResource>>& GetShaderResources() const { return m_ShaderResources; }

		const std::string& GetName() const { return m_Name; }

	private:
		std::unordered_map<ShaderStage, std::string> PreProcess(std::string_view source);
		void Compile(ShaderStage stage, const std::string& source);
		void CompileOrGetCache(const std::unordered_map<ShaderStage, std::string>& sources);
		void CreateProgram();
		void Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytes);

	private:
		ShaderSpecification m_Specification;
		std::string m_Name;

		uint32_t m_RendererID;

		std::unordered_map<ShaderStage, std::vector<uint32_t>> m_ShaderBytes;
		std::unordered_map<uint32_t, std::vector<ShaderResource>> m_ShaderResources;
	};
}
