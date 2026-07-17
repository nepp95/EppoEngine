#pragma once

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    class Sampler
    {
    public:
        Sampler();

        [[nodiscard]] auto GetSampler() const -> nvrhi::SamplerHandle { return m_Sampler; }

    private:
        nvrhi::SamplerHandle m_Sampler = nullptr;
    };
}
