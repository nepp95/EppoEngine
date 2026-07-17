#include "pch.h"
#include "Renderer/Sampler.h"

#include "Renderer/DeviceManager.h"

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
}
