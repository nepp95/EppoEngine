#pragma once

#include <nvrhi/nvrhi.h>

namespace Eppo
{
	struct ShaderInputAttribute
	{
		std::string Name;
		nvrhi::Format Type = nvrhi::Format::UNKNOWN;
		uint32_t Location = 0;
		uint32_t Offset = 0;
	};

	struct ShaderResourceBinding
	{
		std::string Name;
		uint32_t Binding = 0;
		uint32_t ArraySize = 1;
		nvrhi::ShaderType Stage = nvrhi::ShaderType::None;
		nvrhi::ResourceType Type = nvrhi::ResourceType::None;

		auto operator==(const ShaderResourceBinding& other) const -> bool
		{
			return Name == other.Name &&
				Binding == other.Binding &&
				ArraySize == other.ArraySize &&
				Stage == other.Stage &&
				Type == other.Type;
		}

		auto operator!=(const ShaderResourceBinding& other) const -> bool
		{
			return !(*this == other);
		}
	};

	struct PushConstantRange
	{
		uint32_t Binding = 0;
		uint32_t Size = 0;
		nvrhi::ShaderType Stage = nvrhi::ShaderType::None;
	};

	struct ShaderSpecification
	{
		std::string Name;
		bool IsCompute = false;
	};

	class Shader
	{
	public:
        explicit Shader(ShaderSpecification spec);
		virtual ~Shader() = default;

		[[nodiscard]] auto GetShaderHandle(nvrhi::ShaderType type) -> nvrhi::ShaderHandle;
		[[nodiscard]] auto GetInputLayout() -> nvrhi::InputLayoutHandle { return m_InputLayout; }
		[[nodiscard]] auto GetBindingLayouts() const -> const std::unordered_map<uint32_t, nvrhi::BindingLayoutHandle>& { return m_BindingLayouts; }
		[[nodiscard]] auto GetDescriptorTable() const -> nvrhi::DescriptorTableHandle { return m_DescriptorTable; }

		[[nodiscard]] constexpr auto GetName() const -> const std::string& { return m_Specification.Name; }

		static auto Create(ShaderSpecification spec) -> Ref<Shader>;

	protected:
		auto CreateShaderHandles() -> void;
		auto CreateInputLayout() -> void;
		auto CreateBindingLayout() -> void;

	protected:
		ShaderSpecification m_Specification;

		std::unordered_map<nvrhi::ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;
		
		std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>> m_ShaderResources;
		std::unordered_map<uint32_t, nvrhi::BindingLayoutHandle> m_BindingLayouts;
		nvrhi::DescriptorTableHandle m_DescriptorTable = nullptr;

		PushConstantRange m_PushConstants;
		bool m_HasPushConstants = false;

		std::vector<ShaderInputAttribute> m_ShaderInputs;
		uint32_t m_InputAttributeStride = 0;
		nvrhi::InputLayoutHandle m_InputLayout = nullptr;

		// Necessary for compilation/reflection only
		// TODO: We clear them now after use, but maybe remove them from here altogether
		std::unordered_map<nvrhi::ShaderType, std::string> m_ShaderSources;
		std::unordered_map<nvrhi::ShaderType, std::vector<char>> m_ShaderBytes;
	};

	namespace Utils
	{
		auto NvrhiFormatSize(nvrhi::Format format) -> uint32_t;
	}
}