#pragma once

#include "Renderer/DescriptorManager.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    struct SamplerSpecification
    {
        nvrhi::SamplerAddressMode AddressModeU = nvrhi::SamplerAddressMode::Wrap;
        nvrhi::SamplerAddressMode AddressModeV = nvrhi::SamplerAddressMode::Wrap;
        nvrhi::SamplerAddressMode AddressModeW = nvrhi::SamplerAddressMode::Wrap;
        bool MinFilter = true;
        bool MagFilter = true;
        bool MipFilter = true;
        float MaxAnisotropy = 1.0f;

        [[nodiscard]] auto GetKey() const -> uint64_t
        {
            uint64_t key = static_cast<uint64_t>(AddressModeU);
            key |= static_cast<uint64_t>(AddressModeV) << 8;
            key |= static_cast<uint64_t>(AddressModeW) << 16;
            key |= static_cast<uint64_t>(MinFilter) << 24;
            key |= static_cast<uint64_t>(MagFilter) << 25;
            key |= static_cast<uint64_t>(MipFilter) << 26;
            key |= static_cast<uint64_t>(std::bit_cast<uint32_t>(MaxAnisotropy)) << 32;
            return key;
        }
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
