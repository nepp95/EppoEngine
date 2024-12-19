#pragma once

namespace Eppo
{
	// For compatibility with all devices, we only use 4 different sets
	// 
	// Set 0 = Per frame global data
	// Set 1 = Per frame render pass data
	// Set 2 = Per frame material data
	// Set 3 = Per frame object data

	enum class ShaderStage : uint8_t
	{
		None = 0,
		Vertex,
		Fragment,
		All
	};

	enum class ShaderResourceType : uint8_t
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
	};

	class Shader
	{
	public:
		explicit Shader(ShaderSpecification specification);
		virtual ~Shader() = default;

		[[nodiscard]] const ShaderSpecification& GetSpecification() const { return m_Specification; }
		[[nodiscard]] const std::string& GetName() const { return m_Name; }

		static Ref<Shader> Create(const ShaderSpecification& specification);

	private:
		ShaderSpecification m_Specification;
		std::string m_Name;
	};

	namespace Utils
	{
		std::filesystem::path GetOrCreateCacheDirectory();
		std::string ShaderStageToString(ShaderStage stage);
		ShaderStage StringToShaderStage(std::string_view stage);
	}
}
