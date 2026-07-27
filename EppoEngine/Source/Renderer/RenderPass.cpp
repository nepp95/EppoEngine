#include "pch.h"
#include "Renderer/RenderPass.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
    namespace
    {
        auto MakeBindingSetItem(uint32_t binding, nvrhi::IResource* resource, const std::vector<ShaderResourceBinding>& setResources)
            -> nvrhi::BindingSetItem
        {
            if (auto* sampler = dynamic_cast<nvrhi::ISampler*>(resource))
            {
                for (const auto& r : setResources)
                    if (r.Binding == binding && r.Type == nvrhi::ResourceType::Sampler)
                        return nvrhi::BindingSetItem::Sampler(binding, sampler);

                EP_ASSERT(false && "SetInput: no sampler reflected at this binding");
                return nvrhi::BindingSetItem::None(binding);
            }

            if (auto* texture = dynamic_cast<nvrhi::ITexture*>(resource))
            {
                for (const auto& r : setResources)
                    if (r.Binding == binding && r.Type == nvrhi::ResourceType::Texture_SRV)
                        return nvrhi::BindingSetItem::Texture_SRV(binding, texture);

                EP_ASSERT(false && "SetInput: no texture_srv reflected at this binding");
                return nvrhi::BindingSetItem::None(binding);
            }

            if (auto* buffer = dynamic_cast<nvrhi::IBuffer*>(resource))
            {
                for (const auto& r : setResources)
                {
                    if (r.Binding != binding)
                        continue;

                    switch (r.Type)
                    {
                        case nvrhi::ResourceType::ConstantBuffer:
                        case nvrhi::ResourceType::VolatileConstantBuffer:
                            return nvrhi::BindingSetItem::ConstantBuffer(binding, buffer);
                        case nvrhi::ResourceType::StructuredBuffer_SRV:
                            return nvrhi::BindingSetItem::StructuredBuffer_SRV(binding, buffer);
                        case nvrhi::ResourceType::StructuredBuffer_UAV:
                            return nvrhi::BindingSetItem::StructuredBuffer_UAV(binding, buffer);
                        case nvrhi::ResourceType::TypedBuffer_SRV:
                            return nvrhi::BindingSetItem::TypedBuffer_SRV(binding, buffer);
                        case nvrhi::ResourceType::TypedBuffer_UAV:
                            return nvrhi::BindingSetItem::TypedBuffer_UAV(binding, buffer);
                        case nvrhi::ResourceType::RawBuffer_SRV:
                            return nvrhi::BindingSetItem::RawBuffer_SRV(binding, buffer);
                        case nvrhi::ResourceType::RawBuffer_UAV:
                            return nvrhi::BindingSetItem::RawBuffer_UAV(binding, buffer);
                        default:
                            break;
                    }
                }

                EP_ASSERT(false && "SetInput: no buffer reflected at this binding");
                return nvrhi::BindingSetItem::None(binding);
            }

            EP_ASSERT(false && "SetInput: unsupported IResource type");
            return nvrhi::BindingSetItem::None(binding);
        }
    }

    RenderPass::RenderPass(RenderPassSpecification spec)
        : m_Specification(std::move(spec))
    {}

    auto RenderPass::Resize(const uint32_t width, const uint32_t height) const -> void
    {
        if (m_Specification.Pipeline)
            m_Specification.Pipeline->Resize(width, height);
    }

    auto RenderPass::SetInput(const uint32_t set, const uint32_t binding, nvrhi::IResource* resource) -> void
    {
        m_Inputs[set].push_back({ .Binding = binding, .Resource = resource });
    }

    auto RenderPass::DeclarePushConstants(const uint32_t set, const uint32_t size) -> void
    {
        m_PushConstantSizes[set] = size;
    }

    auto RenderPass::Bake() -> void
    {
        m_OwnedBindingSets.clear();
        m_BindingSets = {};

        if (!IsValid())
        {
            Log::Warn("RenderPass::Bake failed because render pass is invalid!");
            m_Inputs.clear();
            m_PushConstantSizes.clear();
            return;
        }

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();
        const auto& descriptorManager = dm->GetRenderer()->GetDescriptorManager();
        const auto& shader = m_Specification.Pipeline->GetSpecification().Shader;
        const auto& layouts = shader->GetBindingLayouts();
        const auto& resources = shader->GetShaderResources();

        uint32_t expectedSet = 0;
        for (const auto& [set, layout] : layouts)
        {
            EP_ASSERT(set == expectedSet);
            ++expectedSet;

            if (const auto* bindlessDesc = layout->getBindlessDesc())
            {
                if (bindlessDesc->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv)
                    m_BindingSets.push_back(descriptorManager->GetResourceDT());
                else if (bindlessDesc->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSampler)
                    m_BindingSets.push_back(descriptorManager->GetSamplerDT());
                else
                    EP_ASSERT(false);
                continue;
            }

            nvrhi::BindingSetDesc desc{};

            if (const auto pcIt = m_PushConstantSizes.find(set); pcIt != m_PushConstantSizes.end())
            {
                const auto& pc = shader->GetPushConstants();
                desc.bindings.push_back(nvrhi::BindingSetItem::PushConstants(pc.Binding, pcIt->second));
            }

            if (const auto inputIt = m_Inputs.find(set); inputIt != m_Inputs.end())
            {
                const auto resourceIt = resources.find(set);
                EP_ASSERT(resourceIt != resources.end());

                for (const auto& input : inputIt->second)
                    desc.bindings.push_back(MakeBindingSetItem(input.Binding, input.Resource, resourceIt->second));
            }

            auto bindingSet = device->createBindingSet(desc, layout);
            EP_ASSERT(bindingSet);
            m_BindingSets.push_back(bindingSet.Get());
            m_OwnedBindingSets.push_back(std::move(bindingSet));
        }

        m_Inputs.clear();
        m_PushConstantSizes.clear();
    }

    auto RenderPass::IsValid() const -> bool
    {
        if (!m_Specification.Pipeline)
            return false;
        if (!m_Specification.Pipeline->GetSpecification().Framebuffer)
            return false;

        return true;
    }
}
