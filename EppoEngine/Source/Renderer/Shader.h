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

    struct ShaderSpecification
    {
        std::string Name;
        bool IsCompute = false;
        // With a source the shader is packed: this and Includes are all it may read, never the filesystem.
        // Without it the shader is compiled from Resources/Shaders. Includes are keyed by path relative to that directory.
        std::string Source;
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
        [[nodiscard]] auto GetShaderSource() const -> const std::string& { return m_ShaderSource; }
        [[nodiscard]] auto GetInputLayout() -> nvrhi::InputLayoutHandle { return m_InputLayout; }

        [[nodiscard]] auto GetShaderResources() const -> const std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>>&
        {
            return m_ShaderResources;
        }

        [[nodiscard]] auto HasPushConstants() const -> bool { return m_HasPushConstants; }
        [[nodiscard]] auto GetPushConstants() const -> const PushConstantRange& { return m_PushConstants; }

        [[nodiscard]] constexpr auto GetName() const -> const std::string& { return m_Specification.Name; }
        [[nodiscard]] auto IsLoaded() const -> bool;

        static auto Create(ShaderSpecification spec) -> Ref<Shader>;

    protected:
        auto CompileOrGetCache() -> bool;
        auto Compile(nvrhi::ShaderType type) -> bool;
        auto CreateShaderHandles() -> void;
        auto CreateInputLayout() -> void;
        auto CreateBindingLayout() -> void;

    protected:
        ShaderSpecification m_Specification;
        std::atomic<bool> m_IsLoaded = false;

        std::unordered_map<nvrhi::ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;

        std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>> m_ShaderResources;
        std::map<uint32_t, nvrhi::BindingLayoutHandle> m_BindingLayouts;

        PushConstantRange m_PushConstants;
        bool m_HasPushConstants = false;

        std::vector<ShaderInputAttribute> m_ShaderInputs;
        uint32_t m_InputAttributeStride = 0;
        nvrhi::InputLayoutHandle m_InputLayout = nullptr;

        std::string m_ShaderSource;
        std::unordered_map<nvrhi::ShaderType, std::vector<char>> m_ShaderBytes;
    };

    namespace Utils
    {
        auto NvrhiFormatSize(nvrhi::Format format) -> uint32_t;
        auto ShaderEntryPoint(nvrhi::ShaderType type) -> const char*;
    }
}
