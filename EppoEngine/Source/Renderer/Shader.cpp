#include "pch.h"
#include "Renderer/Shader.h"

#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
    namespace Utils
    {
        auto NvrhiFormatSize(nvrhi::Format format) -> uint32_t
        {
            switch (format)
            {
                case nvrhi::Format::R32_FLOAT:
                    return 4;
                case nvrhi::Format::RG32_FLOAT:
                    return 4 * 2;
                case nvrhi::Format::RGB32_FLOAT:
                    return 4 * 3;
                case nvrhi::Format::RGBA32_FLOAT:
                    return 4 * 4;
                case nvrhi::Format::R8_UNORM:
                case nvrhi::Format::R8_UINT:
                    return 1;
                case nvrhi::Format::RG8_UNORM:
                    return 1 * 2;
                case nvrhi::Format::RGBA8_UNORM:
                    return 1 * 4;
                case nvrhi::Format::R32_UINT:
                    return 4;

                default:
                {
                    EP_ASSERT(false);
                    return 0;
                }
            }
        }

        auto ShaderEntryPoint(const nvrhi::ShaderType type) -> const char*
        {
            switch (type)
            {
                case nvrhi::ShaderType::Vertex:
                    return "VSMain";
                case nvrhi::ShaderType::Pixel:
                    return "PSMain";
            }

            EP_ASSERT(false);
            return "Main";
        }
    }

    Shader::Shader(ShaderSpecification spec)
        : m_Specification(std::move(spec))
    {
        Log::Info("Loading shader '{}'", m_Specification.Name);

        EP_ASSERT(DeviceManager::Get()->GetParams().API == RendererAPI::Vulkan);
        EP_ASSERT(!m_Specification.IsCompute);
    }

    auto Shader::GetShaderHandle(const nvrhi::ShaderType type) -> nvrhi::ShaderHandle
    {
        if (const auto it = m_ShaderHandles.find(type); it != m_ShaderHandles.end())
            return it->second;
        return nullptr;
    }

    auto Shader::Create(ShaderSpecification spec) -> Ref<Shader>
    {
        switch (const auto& dm = DeviceManager::Get(); dm->GetParams().API)
        {
            case RendererAPI::Vulkan:
                return CreateRef<VulkanShader>(std::move(spec));

            default:
            {
                EP_ASSERT(false);
                return nullptr;
                break;
            }
        }
    }

    auto Shader::CreateShaderHandles() -> void
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::ShaderDesc shaderDesc{};

        for (const auto& [type, bytes] : m_ShaderBytes)
        {
            shaderDesc.shaderType = type;
            shaderDesc.entryName = Utils::ShaderEntryPoint(type);
            m_ShaderHandles[type] = device->createShader(shaderDesc, m_ShaderBytes.at(type).data(), m_ShaderBytes.at(type).size());
        }
    }

    auto Shader::CreateInputLayout() -> void
    {
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();

        std::vector<nvrhi::VertexAttributeDesc> attributeDescs(m_ShaderInputs.size());
        for (uint32_t i = 0; i < m_ShaderInputs.size(); i++)
        {
            auto& input = m_ShaderInputs.at(i);
            auto& attributeDesc = attributeDescs.at(i);

            attributeDesc.name = input.Name;
            attributeDesc.format = input.Type;
            attributeDesc.arraySize = 1;
            attributeDesc.bufferIndex = 0;
            attributeDesc.offset = input.Offset;
            attributeDesc.elementStride = m_InputAttributeStride;
            attributeDesc.isInstanced = false;
        }

        m_InputLayout = device->createInputLayout(
            attributeDescs.data(), static_cast<uint32_t>(attributeDescs.size()), m_ShaderHandles.at(nvrhi::ShaderType::Vertex)
        );
    }

    auto Shader::CreateBindingLayout() -> void
    {
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();
        const auto& descriptorManager = dm->GetRenderer()->GetDescriptorManager();
        const auto isGlobalHeap = [this](const uint32_t set, const std::string_view name, const nvrhi::ResourceType type) -> bool
        {
            const auto it = m_ShaderResources.find(set);
            if (it == m_ShaderResources.end())
                return true;

            return it->second.size() == 1 && it->second.front().Name == name && it->second.front().Type == type;
        };

        if (!isGlobalHeap(1, "ResourceDescriptorHeap", nvrhi::ResourceType::Texture_SRV) ||
            !isGlobalHeap(2, "SamplerDescriptorHeap", nvrhi::ResourceType::Sampler))
        {
            Log::Error("Shader '{}' uses descriptor sets reserved for the global bindless heaps!", m_Specification.Name);
            EP_ASSERT(false);
            return;
        }

        for (const auto& [set, setResources] : m_ShaderResources)
        {
            if (set == 1 || set == 2)
                continue;

            nvrhi::BindingLayoutDesc bindingLayoutDesc{
                .visibility = nvrhi::ShaderType::All,
            };

            if (set == 0 && m_HasPushConstants)
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(m_PushConstants.Binding, m_PushConstants.Size));

            for (const auto& resource : setResources)
            {
                if (resource.ArraySize == 0)
                {
                    EP_ASSERT(false);
                    continue;
                }

                nvrhi::BindingLayoutItem item{
                    .slot = resource.Binding,
                    .type = resource.Type,
                    .size = static_cast<uint16_t>(resource.ArraySize),
                };

                bindingLayoutDesc.addItem(item);
            }

            if (!bindingLayoutDesc.bindings.empty())
                m_BindingLayouts[set] = device->createBindingLayout(bindingLayoutDesc);
        }

        if (!m_BindingLayouts.contains(0))
        {
            nvrhi::BindingLayoutDesc bindingLayoutDesc{
                .visibility = nvrhi::ShaderType::All,
            };
            if (m_HasPushConstants)
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(m_PushConstants.Binding, m_PushConstants.Size));
            m_BindingLayouts[0] = device->createBindingLayout(bindingLayoutDesc);
        }

        EP_ASSERT(!m_BindingLayouts.contains(1));
        EP_ASSERT(!m_BindingLayouts.contains(2));
        m_BindingLayouts[1] = descriptorManager->GetResourceHeap()->BindingLayout;
        m_BindingLayouts[2] = descriptorManager->GetSamplerHeap()->BindingLayout;
    }
}
