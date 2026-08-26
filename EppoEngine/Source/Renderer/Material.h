#pragma once

#include "Renderer/DescriptorManager.h"

namespace Eppo
{
    enum class MaterialAlphaMode : uint32_t
    {
        Opaque = 0, // Alpha ignored
        Mask = 1, // Not stored in G-Buffer
        Blend = 2, // Processed by transparency shader
    };

    struct Material
    {
        Ref<BindlessHandle> DiffuseMap = nullptr;
        Ref<BindlessHandle> NormalMap = nullptr;
        Ref<BindlessHandle> RoughMetMap = nullptr;
        Ref<BindlessHandle> AOMap = nullptr;
        Ref<BindlessHandle> EmissiveMap = nullptr;
        Ref<Sampler> DiffuseSampler = nullptr;
        Ref<Sampler> NormalSampler = nullptr;
        Ref<Sampler> RoughMetSampler = nullptr;
        Ref<Sampler> AOSampler = nullptr;
        Ref<Sampler> EmissiveSampler = nullptr;
        glm::vec3 EmissiveFactor = glm::vec3(0.0f);
        glm::vec4 BaseColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        float Roughness = 1.0f;
        float Metallic = 0.0f;
        float NormalScale = 1.0f;

        MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
        float AlphaCutoff = 0.5f;
        bool DoubleSided = false;

        // NOTE: This converts a uint32 to a int32 which loses half the range.
        //       Currently this is no issue since our handles won't ever reach that far,
        //       But this might change in the future.
        [[nodiscard]] auto GetDiffuseMapIndex() const -> int32_t { return DiffuseMap ? static_cast<int32_t>(DiffuseMap->Index) : -1; }
        [[nodiscard]] auto GetNormalMapIndex() const -> int32_t { return NormalMap ? static_cast<int32_t>(NormalMap->Index) : -1; }
        [[nodiscard]] auto GetRoughMetMapIndex() const -> int32_t { return RoughMetMap ? static_cast<int32_t>(RoughMetMap->Index) : -1; }
        [[nodiscard]] auto GetAOMapIndex() const -> int32_t { return AOMap ? static_cast<int32_t>(AOMap->Index) : -1; }
        [[nodiscard]] auto GetEmissiveMapIndex() const -> int32_t { return EmissiveMap ? static_cast<int32_t>(EmissiveMap->Index) : -1; }
    };
}
