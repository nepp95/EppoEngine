#pragma once

#include "Renderer/DeviceManager.h"
#include "Renderer/Image.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    class DescriptorManager;
    class Sampler;

    enum class BindlessHeapType
    {
        Resource,
        Sampler,
    };

    struct BindlessHandle
    {
        WeakRef<DescriptorManager> Manager;
        uint32_t Index = std::numeric_limits<uint32_t>::max();
        BindlessHeapType HeapType = BindlessHeapType::Resource;

        BindlessHandle() = default;
        BindlessHandle(const Ref<DescriptorManager>& manager, const uint32_t index, const BindlessHeapType heapType);
        ~BindlessHandle();
        BindlessHandle(const BindlessHandle&) = delete;
        BindlessHandle& operator=(const BindlessHandle&) = delete;
        BindlessHandle(BindlessHandle&& other) noexcept;
        auto operator=(BindlessHandle&& other) noexcept -> BindlessHandle&;
    };

    template<typename T>
    concept ResourceType = std::same_as<T, Image> || std::same_as<T, UniformBuffer> || std::same_as<T, StorageBuffer>;

    class DescriptorManager : public std::enable_shared_from_this<DescriptorManager>
    {
    public:
        explicit DescriptorManager();

        struct Heap
        {
            nvrhi::BindingLayoutHandle BindingLayout = nullptr;
            nvrhi::DescriptorTableHandle DescriptorTable = nullptr;
            uint32_t Capacity = 0;
            uint32_t NextFreeSlot = 0;
            std::vector<uint32_t> FreeList;
            std::mutex Mutex;
        };

        template<ResourceType T>
        [[nodiscard]] auto Register(const Ref<T>& resource) -> BindlessHandle
        {
            const auto& dm = DeviceManager::Get();
            const auto device = dm->GetDevice();

            const std::scoped_lock lock(m_ResourceHeap->Mutex);

            // Resize if full
            const uint32_t slot = GetNextSlot(device, m_ResourceHeap);
            if (slot == std::numeric_limits<uint32_t>::max())
                return {};

            // Write to table
            nvrhi::BindingSetItem item;
            if constexpr (std::same_as<T, Image>)
                item = nvrhi::BindingSetItem::Texture_SRV(slot, resource->GetTexture(), resource->GetFormat());
            if constexpr (std::same_as<T, UniformBuffer>)
                item = nvrhi::BindingSetItem::ConstantBuffer(slot, resource->GetBuffer());
            if constexpr (std::same_as<T, StorageBuffer>)
                item = nvrhi::BindingSetItem::StructuredBuffer_SRV(slot, resource->GetBuffer());

            EP_ASSERT(m_ResourceHeap->DescriptorTable->getCapacity() > slot);
            device->writeDescriptorTable(m_ResourceHeap->DescriptorTable, item);

            return { shared_from_this(), slot, BindlessHeapType::Resource };
        }

        [[nodiscard]] auto Register(const Ref<Sampler>& resource) -> BindlessHandle;

        [[nodiscard]] auto GetResourceHeap() const -> const ScopedPtr<Heap>& { return m_ResourceHeap; }
        [[nodiscard]] auto GetSamplerHeap() const -> const ScopedPtr<Heap>& { return m_SamplerHeap; }
        [[nodiscard]] auto GetResourceDT() const -> nvrhi::DescriptorTableHandle;
        [[nodiscard]] auto GetSamplerDT() const -> nvrhi::DescriptorTableHandle;

    private:
        auto Release(BindlessHeapType type, uint32_t index) const -> void;

        // Internally reallocates the descriptor table if necessary
        static auto GetNextSlot(nvrhi::IDevice* device, const ScopedPtr<Heap>& heap) -> uint32_t;

    private:
        ScopedPtr<Heap> m_ResourceHeap = nullptr;
        ScopedPtr<Heap> m_SamplerHeap = nullptr;

        static constexpr uint32_t s_MaxSlots = 16384;
        static constexpr uint32_t s_MaxSamplerSlots = 2048; // Limited by DX12

        friend struct BindlessHandle;
    };
}
