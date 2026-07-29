#include "pch.h"
#include "Renderer/Sampler.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
    Sampler::Sampler(const SamplerSpecification& specification)
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::SamplerDesc samplerDesc{};
        samplerDesc.addressU = specification.AddressModeU;
        samplerDesc.addressV = specification.AddressModeV;
        samplerDesc.addressW = specification.AddressModeW;
        samplerDesc.setAllFilters(specification.AllFilters);
        m_Sampler = device->createSampler(samplerDesc);
    }

    auto Sampler::Create(const SamplerSpecification& specification, const Ref<DescriptorManager>& descriptorManager) -> Ref<Sampler>
    {
        const auto sampler = Ref<Sampler>(new Sampler(specification));
        const auto& manager = descriptorManager ? descriptorManager : DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
        sampler->m_BindlessHandle = manager->Register(sampler);
        return sampler;
    }
}
