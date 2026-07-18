#pragma once

#include "Renderer/DescriptorManager.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    class Sampler
    {
    public:
        [[nodiscard]] static auto Create(const Ref<DescriptorManager>& descriptorManager = nullptr) -> Ref<Sampler>;

        [[nodiscard]] auto GetSampler() const -> nvrhi::SamplerHandle { return m_Sampler; }
        [[nodiscard]] auto GetBindlessHandle() const -> const BindlessHandle& { return m_BindlessHandle; }
        [[nodiscard]] auto GetBindlessIndex() const -> uint32_t { return m_BindlessHandle.Index; }

    private:
        Sampler();

        nvrhi::SamplerHandle m_Sampler = nullptr;
        BindlessHandle m_BindlessHandle;
    };
}
