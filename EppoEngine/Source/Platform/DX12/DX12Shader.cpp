#include "pch.h"
#include "Platform/DX12/DX12Shader.h"

#include "Platform/ComPtr.h"

#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <nvrhi/utils.h>

#include <bit>

namespace Eppo
{
    namespace
    {
        auto DxilTypeToNvrhiType(const std::string& semantic, const D3D12_SIGNATURE_PARAMETER_DESC& type) -> nvrhi::Format
        {
            const uint32_t components = std::popcount(static_cast<uint32_t>(type.Mask));

            bool packed = semantic.substr(0, 5) == "COLOR";

            switch (type.ComponentType)
            {
                case D3D_REGISTER_COMPONENT_FLOAT32:
                {
                    if (components == 1)
                        return packed ? nvrhi::Format::R8_UNORM : nvrhi::Format::R32_FLOAT;
                    if (components == 2)
                        return packed ? nvrhi::Format::RG8_UNORM : nvrhi::Format::RG32_FLOAT;
                    if (components == 3)
                        return nvrhi::Format::RGB32_FLOAT;
                    if (components == 4)
                        return packed ? nvrhi::Format::RGBA8_UNORM : nvrhi::Format::RGBA32_FLOAT;

                    EP_ASSERT(false);
                    return nvrhi::Format::UNKNOWN;
                }

                case D3D_REGISTER_COMPONENT_UINT32:
                    return nvrhi::Format::R32_UINT;

                default:
                {
                    EP_ASSERT(false);
                    return nvrhi::Format::UNKNOWN;
                }
            }
        }
    }

    DX12Shader::DX12Shader(ShaderSpecification spec)
        : Shader(std::move(spec))
    {
        if (!CompileOrGetCache())
        {
            Log::Error("Shader '{}' could not be compiled!", m_Specification.Name);
            EP_ASSERT(false, "Shader compilation failed!");
            return;
        }

        CreateShaderHandles();

        Log::Info("==================================");
        Log::Info("===== Shader Reflection Data =====");
        Log::Info("==================================");
        Log::Info("Name: {}", m_Specification.Name);

        for (const auto& [type, bytes] : m_ShaderBytes)
        {
            if (!Reflect(type))
            {
                EP_ASSERT(false, "Shader reflection failed!");
                return;
            }
        }

        if (m_ShaderBytes.contains(nvrhi::ShaderType::Vertex))
            CreateInputLayout();

        Log::Info("==================================");

        CreateBindingLayout();

        m_IsLoaded.store(true, std::memory_order_release);
    }

    auto DX12Shader::Reflect(const nvrhi::ShaderType type) -> bool
    {
        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcContainerReflection> container;
        ComPtr<IDxcBlobEncoding> blob;
        ComPtr<ID3D12ShaderReflection> reflection;
        UINT32 part = 0;
        const auto& bytes = m_ShaderBytes.at(type);
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
            FAILED(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container))) ||
            FAILED(utils->CreateBlob(bytes.data(), static_cast<uint32_t>(bytes.size()), DXC_CP_ACP, &blob)) ||
            FAILED(container->Load(blob.Get())) ||
            FAILED(container->FindFirstPartKind(DXC_PART_DXIL, &part)) ||
            FAILED(container->GetPartReflection(part, IID_PPV_ARGS(&reflection))))
        {
            Log::Error("Could not reflect shader '{}'!", m_Specification.Name);
            return false;
        }

        D3D12_SHADER_DESC shaderDesc{};
        if (FAILED(reflection->GetDesc(&shaderDesc)))
        {
            Log::Error("Could not read reflection data for shader '{}'!", m_Specification.Name);
            return false;
        }

        std::unordered_map<D3D_SHADER_INPUT_TYPE, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> resources;
        for (uint32_t index = 0; index < shaderDesc.BoundResources; index++)
        {
            D3D12_SHADER_INPUT_BIND_DESC resource{};
            if (FAILED(reflection->GetResourceBindingDesc(index, &resource)))
            {
                Log::Error("Could not read resource {} for shader '{}'!", index, m_Specification.Name);
                return false;
            }
            resources[resource.Type].push_back(resource);
        }

        Log::Info("Stage: {}", nvrhi::utils::ShaderStageToString(type));

        if (shaderDesc.InputParameters != 0 && type == nvrhi::ShaderType::Vertex)
        {
            Log::Info("\tInputs:");

            for (uint32_t index = 0; index < shaderDesc.InputParameters; index++)
            {
                D3D12_SIGNATURE_PARAMETER_DESC resource{};
                if (FAILED(reflection->GetInputParameterDesc(index, &resource)))
                {
                    Log::Error("Could not read input {} for shader '{}'!", index, m_Specification.Name);
                    return false;
                }
                if (resource.SystemValueType != D3D_NAME_UNDEFINED)
                    continue;

                auto& input = m_ShaderInputs.emplace_back();
                input.Name = resource.SemanticName;
                input.Location = resource.Register;
                input.Type = DxilTypeToNvrhiType(resource.SemanticName, resource);
                input.Offset = m_InputAttributeStride;

                m_InputAttributeStride += Utils::NvrhiFormatSize(input.Type);

                Log::Info("\t\tName: {}", input.Name);
                Log::Info("\t\tLocation: {}", input.Location);
                Log::Info("\t\tType: {}", nvrhi::utils::FormatToString(input.Type));
            }

            std::ranges::sort(
                m_ShaderInputs, std::ranges::less{},
                [](const ShaderInputAttribute& input) -> uint32_t
                {
                    return input.Location;
                }
            );
        }

        for (const auto& resource : resources[D3D_SIT_CBUFFER])
        {
            if (std::string_view(resource.Name) != "uPC")
                continue;

            const auto constantBuffer = reflection->GetConstantBufferByName(resource.Name);
            D3D12_SHADER_BUFFER_DESC bufferDesc{};
            if (FAILED(constantBuffer->GetDesc(&bufferDesc)) || resource.BindPoint != 0 || resource.Space != 0)
            {
                Log::Error("Could not reflect push constants at b0, space0 for shader '{}'!", m_Specification.Name);
                return false;
            }

            uint32_t pushConstantSize = 0;
            for (uint32_t index = 0; index < bufferDesc.Variables; index++)
            {
                D3D12_SHADER_VARIABLE_DESC variableDesc{};
                if (FAILED(constantBuffer->GetVariableByIndex(index)->GetDesc(&variableDesc)))
                {
                    Log::Error("Could not read push constant {} for shader '{}'!", index, m_Specification.Name);
                    return false;
                }
                pushConstantSize = std::max(pushConstantSize, variableDesc.StartOffset + variableDesc.Size);
            }

            m_PushConstants.Binding = 0;
            if (pushConstantSize > m_PushConstants.Size)
                m_PushConstants.Size = pushConstantSize;
            m_PushConstants.Stage = m_HasPushConstants ? nvrhi::ShaderType::All : type;
            m_HasPushConstants = true;
        }

        if (!resources[D3D_SIT_CBUFFER].empty())
        {
            Log::Info("Found {} uniform_buffers", resources[D3D_SIT_CBUFFER].size());

            for (const auto& resource : resources[D3D_SIT_CBUFFER])
            {
                if (std::string_view(resource.Name) == "uPC")
                    continue;

                const uint32_t set = resource.Space;
                const uint32_t binding = resource.BindPoint;

                bool bindingExists = false;
                if (m_ShaderResources.contains(set))
                {
                    for (auto& setResource : m_ShaderResources.at(set))
                    {
                        if (resource.Name == setResource.Name && binding == setResource.Binding)
                        {
                            setResource.Stage = (setResource.Stage | type);
                            bindingExists = true;
                            break;
                        }
                    }
                }

                if (!bindingExists)
                {
                    ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
                    shaderResource.Name = resource.Name;
                    shaderResource.Binding = binding;
                    shaderResource.Stage = type;
                    shaderResource.Type = nvrhi::ResourceType::ConstantBuffer;

                    Log::Info("\t\tName: {}", shaderResource.Name);
                    Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
                    Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
                }
            }
        }

        if (!resources[D3D_SIT_TEXTURE].empty())
        {
            Log::Info("Found {} separate_images", resources[D3D_SIT_TEXTURE].size());

            for (const auto& resource : resources[D3D_SIT_TEXTURE])
            {
                const uint32_t set = resource.Space;
                const uint32_t binding = resource.BindPoint;

                const uint32_t arraySize = resource.BindCount;

                bool bindingExists = false;
                if (m_ShaderResources.contains(set))
                {
                    for (auto& setResource : m_ShaderResources.at(set))
                    {
                        if (resource.Name == setResource.Name && binding == setResource.Binding)
                        {
                            setResource.Stage = (setResource.Stage | type);
                            bindingExists = true;
                            break;
                        }
                    }
                }

                if (!bindingExists)
                {
                    ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
                    shaderResource.Name = resource.Name;
                    shaderResource.Binding = binding;
                    shaderResource.ArraySize = arraySize;
                    shaderResource.Stage = type;
                    shaderResource.Type = nvrhi::ResourceType::Texture_SRV;

                    Log::Info("\t\tName: {}", shaderResource.Name);
                    Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
                    Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
                }
            }
        }

        if (!resources[D3D_SIT_SAMPLER].empty())
        {
            Log::Info("Found {} separate_samplers", resources[D3D_SIT_SAMPLER].size());

            for (const auto& resource : resources[D3D_SIT_SAMPLER])
            {
                const uint32_t set = resource.Space;
                const uint32_t binding = resource.BindPoint;

                bool bindingExists = false;
                if (m_ShaderResources.contains(set))
                {
                    for (auto& setResource : m_ShaderResources.at(set))
                    {
                        if (resource.Name == setResource.Name && binding == setResource.Binding)
                        {
                            setResource.Stage = (setResource.Stage | type);
                            bindingExists = true;
                            break;
                        }
                    }
                }

                if (!bindingExists)
                {
                    ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
                    shaderResource.Name = resource.Name;
                    shaderResource.Binding = binding;
                    shaderResource.Stage = type;
                    shaderResource.Type = nvrhi::ResourceType::Sampler;

                    Log::Info("\t\tName: {}", shaderResource.Name);
                    Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
                    Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
                }
            }
        }

        if (!resources[D3D_SIT_STRUCTURED].empty())
        {
            Log::Info("Found {} storage_buffers", resources[D3D_SIT_STRUCTURED].size());

            for (const auto& resource : resources[D3D_SIT_STRUCTURED])
            {
                const uint32_t set = resource.Space;
                const uint32_t binding = resource.BindPoint;

                bool bindingExists = false;
                if (m_ShaderResources.contains(set))
                {
                    for (auto& setResource : m_ShaderResources.at(set))
                    {
                        if (resource.Name == setResource.Name && binding == setResource.Binding)
                        {
                            setResource.Stage = (setResource.Stage | type);
                            bindingExists = true;
                            break;
                        }
                    }
                }

                if (!bindingExists)
                {
                    ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
                    shaderResource.Name = resource.Name;
                    shaderResource.Binding = binding;
                    shaderResource.Stage = type;
                    shaderResource.Type = nvrhi::ResourceType::StructuredBuffer_SRV;

                    Log::Info("\t\tName: {}", shaderResource.Name);
                    Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
                    Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
                }
            }
        }
        Log::Trace("Found {} sampled_images", 0);
        Log::Trace("Found {} storage_images", resources[D3D_SIT_UAV_RWTYPED].size());
        return true;
    }
}
