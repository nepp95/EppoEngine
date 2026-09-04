#include "pch.h"
#include "Renderer/RenderPass.h"

#include "Renderer/Buffer/StorageBuffer.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"
#include "Renderer/Sampler.h"

namespace Eppo
{
    RenderPass::RenderPass(RenderPassSpecification spec)
        : m_Specification(std::move(spec))
    {}

    auto RenderPass::Resize(const uint32_t width, const uint32_t height) const -> void
    {
        if (m_Specification.OwnsFramebuffer)
            m_Specification.Framebuffer->Resize(width, height);
    }

    auto RenderPass::SetFramebuffer(const Ref<Framebuffer>& framebuffer) -> void
    {
        const bool compatible = m_Specification.Pipeline->IsCompatible(framebuffer);
        EP_ASSERT(compatible);
        m_Specification.Framebuffer = framebuffer;
    }

    auto RenderPass::SetInput(const uint32_t set, const uint32_t binding, const Ref<Image>& resource) -> void
    {
        SetInputInternal(set, binding, nvrhi::ResourceType::Texture_SRV, resource);
    }

    auto RenderPass::SetInput(const uint32_t set, const uint32_t binding, const Ref<Sampler>& resource) -> void
    {
        SetInputInternal(set, binding, nvrhi::ResourceType::Sampler, resource);
    }

    auto RenderPass::SetInput(const uint32_t set, const uint32_t binding, const Ref<StorageBuffer>& resource) -> void
    {
        SetInputInternal(set, binding, nvrhi::ResourceType::StructuredBuffer_SRV, resource);
    }

    auto RenderPass::SetInput(const uint32_t set, const uint32_t binding, const Ref<UniformBuffer>& resource) -> void
    {
        SetInputInternal(set, binding, nvrhi::ResourceType::ConstantBuffer, resource);
    }

    auto RenderPass::Invalidate() -> void
    {
        m_Invalidated = true;
    }

    auto RenderPass::Bake() -> void
    {
        if (!m_Specification.Pipeline)
        {
            Log::Warn("RenderPass::Bake failed because it has no pipeline!");
            return;
        }

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();
        const auto& descriptorManager = dm->GetRenderer()->GetDescriptorManager();
        const auto& shader = m_Specification.Pipeline->GetSpecification().Shader;
        const auto& layouts = shader->GetBindingLayouts();

        std::unordered_map<uint32_t, nvrhi::BindingSetDesc> bindingSetDescs;
        for (const auto& [set, layout] : layouts)
        {
            if (layout->getBindlessDesc())
                continue;

            nvrhi::BindingSetDesc desc{};

            if (const auto& pc = shader->GetPushConstants(); set == 0 && pc.Size > 0)
                desc.bindings.push_back(nvrhi::BindingSetItem::PushConstants(pc.Binding, pc.Size));

            if (const auto inputIt = m_Inputs.find(set); inputIt != m_Inputs.end())
            {
                for (const auto& input : inputIt->second)
                {
                    switch (input.Type)
                    {
                        case nvrhi::ResourceType::Texture_SRV:
                        {
                            const auto* image = static_cast<const Image*>(input.Owner.get());
                            desc.bindings.push_back(
                                nvrhi::BindingSetItem::Texture_SRV(input.Binding, image->GetTexture(), image->GetFormat())
                            );
                            break;
                        }
                        case nvrhi::ResourceType::Sampler:
                        {
                            const auto* sampler = static_cast<const Sampler*>(input.Owner.get());
                            desc.bindings.push_back(nvrhi::BindingSetItem::Sampler(input.Binding, sampler->GetSampler()));
                            break;
                        }
                        case nvrhi::ResourceType::StructuredBuffer_SRV:
                        {
                            const auto* storageBuffer = static_cast<const StorageBuffer*>(input.Owner.get());
                            desc.bindings.push_back(nvrhi::BindingSetItem::StructuredBuffer_SRV(input.Binding, storageBuffer->GetBuffer()));
                            break;
                        }
                        case nvrhi::ResourceType::ConstantBuffer:
                        {
                            const auto* uniformBuffer = static_cast<const UniformBuffer*>(input.Owner.get());
                            desc.bindings.push_back(nvrhi::BindingSetItem::ConstantBuffer(input.Binding, uniformBuffer->GetBuffer()));
                            break;
                        }
                        default:
                            EP_ASSERT(false, "Unsupported render pass input type.");
                            break;
                    }
                }
            }

            bindingSetDescs.emplace(set, std::move(desc));
        }

        if (!m_Invalidated && bindingSetDescs == m_BakedBindingSetDescs)
            return;

        std::vector<nvrhi::BindingSetHandle> ownedBindingSets;
        nvrhi::BindingSetVector bindingSets;

        uint32_t expectedSet = 0;
        for (const auto& [set, layout] : layouts)
        {
            EP_ASSERT(set == expectedSet);
            ++expectedSet;

            if (const auto* bindlessDesc = layout->getBindlessDesc())
            {
                if (bindlessDesc->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv)
                    bindingSets.push_back(descriptorManager->GetResourceDT());
                else if (bindlessDesc->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSampler)
                    bindingSets.push_back(descriptorManager->GetSamplerDT());
                else
                    EP_ASSERT(false);
                continue;
            }

            auto bindingSet = device->createBindingSet(bindingSetDescs.at(set), layout);
            if (!bindingSet)
            {
                Log::Error("Failed to create binding set for render pass '{}' at set {}.", m_Specification.Name, set);
                EP_ASSERT(false, "Render pass inputs do not match the shader's binding layout.");
                return;
            }

            bindingSets.push_back(bindingSet.Get());
            ownedBindingSets.push_back(std::move(bindingSet));
        }

        m_OwnedBindingSets = std::move(ownedBindingSets);
        m_BindingSets = std::move(bindingSets);
        m_BakedBindingSetDescs = std::move(bindingSetDescs);
        m_Invalidated = false;
    }

    auto RenderPass::SetInputInternal(const uint32_t set, const uint32_t binding, const nvrhi::ResourceType type, const Ref<void>& resource)
        -> void
    {
        EP_ASSERT(resource != nullptr, "Cannot bind a null resource to a render pass.");
        if (!resource)
            return;

        auto& inputs = m_Inputs[set];
        for (auto& input : inputs)
        {
            if (input.Binding != binding || input.Type != type)
                continue;

            if (input.Owner.get() == resource.get())
                return;

            input.Owner = resource;
            Invalidate();
            return;
        }

        inputs.push_back(
            {
                .Binding = binding,
                .Type = type,
                .Owner = resource,
            }
        );
        Invalidate();
    }

    auto RenderPass::IsValid() const -> bool
    {
        if (!m_Specification.Pipeline || !m_Specification.Framebuffer)
            return false;

        return m_Specification.Pipeline->IsCompatible(m_Specification.Framebuffer);
    }
}
