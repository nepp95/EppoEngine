#pragma once

#include <nvrhi/nvrhi.h>

#include <map>

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
            return Name == other.Name && Binding == other.Binding && ArraySize == other.ArraySize && Stage == other.Stage &&
                Type == other.Type;
        }

        auto operator!=(const ShaderResourceBinding& other) const -> bool { return !(*this == other); }
    };

    struct PushConstantRange
    {
        uint32_t Binding = 0;
        uint32_t Size = 0;
        nvrhi::ShaderType Stage = nvrhi::ShaderType::None;
    };

    struct PackedShaderData
    {
        std::unordered_map<nvrhi::ShaderType, std::string> ShaderSources;
    };

    struct ShaderSpecification
    {
        std::string Name;
        bool IsCompute = false;
        // With sources the shader is packed: these and Includes are all it may read, never the filesystem.
        // Without them it is compiled from Resources/Shaders. Includes are keyed by path relative to that directory.
        std::unordered_map<nvrhi::ShaderType, std::string> Sources;
        std::map<std::string, std::string> Includes;
    };

    class Shader
    {
    public:
        explicit Shader(ShaderSpecification spec);
        virtual ~Shader() = default;

        [[nodiscard]] auto GetShaderHandle(nvrhi::ShaderType type) -> nvrhi::ShaderHandle;
        // Ordered by ascending set: nvrhi legacy mode maps a set to its index in the pipeline's layout array.
        [[nodiscard]] auto GetBindingLayouts() const -> const std::map<uint32_t, nvrhi::BindingLayoutHandle>& { return m_BindingLayouts; }
        [[nodiscard]] auto GetShaderSources() const -> const std::unordered_map<nvrhi::ShaderType, std::string>& { return m_ShaderSources; }
        [[nodiscard]] auto GetInputLayout() -> nvrhi::InputLayoutHandle { return m_InputLayout; }

        [[nodiscard]] auto GetShaderResources() const -> const std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>>&
        {
            return m_ShaderResources;
        }
        [[nodiscard]] auto GetPushConstants() const -> const PushConstantRange& { return m_PushConstants; }

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
        std::map<uint32_t, nvrhi::BindingLayoutHandle> m_BindingLayouts;

        PushConstantRange m_PushConstants;
        bool m_HasPushConstants = false;

        std::vector<ShaderInputAttribute> m_ShaderInputs;
        uint32_t m_InputAttributeStride = 0;
        nvrhi::InputLayoutHandle m_InputLayout = nullptr;

        std::unordered_map<nvrhi::ShaderType, std::string> m_ShaderSources;
        std::unordered_map<nvrhi::ShaderType, std::vector<char>> m_ShaderBytes;
    };

    namespace Utils
    {
        auto NvrhiFormatSize(nvrhi::Format format) -> uint32_t;
    }
}
