#pragma once

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
    struct DescriptorLayoutInfo
    {
        std::vector<VkDescriptorSetLayoutBinding> Bindings;

        bool operator==(const DescriptorLayoutInfo& other) const;
        [[nodiscard]] size_t hash() const;
    };

    struct DescriptorLayoutHash
    {
        std::size_t operator()(const DescriptorLayoutInfo& k) const
        {
            return k.hash();
        }
    };

    class DescriptorLayoutBuilder
    {
    public:
        void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count);
        void Clear();

        VkDescriptorSetLayout Build(VkShaderStageFlags shaderStageFlags, VkDescriptorSetLayoutCreateFlags createFlags = 0, const void* pNext = nullptr);

    private:
        DescriptorLayoutInfo m_CurrentLayoutInfo;

        // Shaders can be compiled async and use the builder to create or fetch the descriptor set layout during compilation
        static inline std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_DescriptorLayoutCache;
        static inline std::mutex m_Mutex;
    };
}
