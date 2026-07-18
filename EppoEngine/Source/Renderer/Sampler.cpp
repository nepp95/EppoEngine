#include "pch.h"
#include "Renderer/Sampler.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
    Sampler::Sampler()
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::SamplerDesc samplerDesc{};
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
        samplerDesc.setAllFilters(true);
        m_Sampler = device->createSampler(samplerDesc);
    }

    auto Sampler::Create(const Ref<DescriptorManager>& descriptorManager) -> Ref<Sampler>
    {
        const auto sampler = Ref<Sampler>(new Sampler());
        const auto& manager = descriptorManager ? descriptorManager : DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
        sampler->m_BindlessHandle = manager->Register(sampler);
        return sampler;
    }
}
