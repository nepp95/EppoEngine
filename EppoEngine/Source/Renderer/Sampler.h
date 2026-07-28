#pragma once

#include "Renderer/DescriptorManager.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    struct SamplerSpecification
    {
        nvrhi::SamplerAddressMode AddressMode = nvrhi::SamplerAddressMode::Wrap;
        bool AllFilters = true;
    };

    class Sampler
    {
    public:
        [[nodiscard]] static auto
        Create(const SamplerSpecification& specification = {}, const Ref<DescriptorManager>& descriptorManager = nullptr) -> Ref<Sampler>;

        [[nodiscard]] auto GetSampler() const -> nvrhi::SamplerHandle { return m_Sampler; }
        [[nodiscard]] auto GetBindlessHandle() const -> const BindlessHandle& { return m_BindlessHandle; }
        [[nodiscard]] auto GetBindlessIndex() const -> uint32_t { return m_BindlessHandle.Index; }

    private:
        explicit Sampler(const SamplerSpecification& specification);

        nvrhi::SamplerHandle m_Sampler = nullptr;
        BindlessHandle m_BindlessHandle;
    };
}
