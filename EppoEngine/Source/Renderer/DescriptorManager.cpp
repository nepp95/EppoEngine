#include "pch.h"
#include "Renderer/DescriptorManager.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Sampler.h"

namespace Eppo
{
    BindlessHandle::BindlessHandle(const Ref<DescriptorManager>& manager, const uint32_t index, const BindlessHeapType heapType)
        : Manager(manager), Index(index), HeapType(heapType)
    {

    }

    BindlessHandle::BindlessHandle(BindlessHandle&& other) noexcept
    {
        Manager = std::exchange(other.Manager, {});
        Index = std::exchange(other.Index, std::numeric_limits<uint32_t>::max());
        HeapType = std::exchange(other.HeapType, BindlessHeapType::Resource);
    }

    auto BindlessHandle::operator=(BindlessHandle&& other) noexcept -> BindlessHandle&
    {
        if (this != &other)
        {
            if (Index != std::numeric_limits<uint32_t>::max())
            {
                if (const auto manager = Manager.lock())
                    manager->Release(HeapType, Index);
            }

            Manager = std::exchange(other.Manager, {});
            Index = std::exchange(other.Index, std::numeric_limits<uint32_t>::max());
            HeapType = std::exchange(other.HeapType, BindlessHeapType::Resource);
        }

        return *this;
    }

    BindlessHandle::~BindlessHandle()
    {
        if (Index == std::numeric_limits<uint32_t>::max())
            return;
        if (const auto manager = Manager.lock())
            manager->Release(HeapType, Index);
    }

    DescriptorManager::DescriptorManager()
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        constexpr uint32_t initialHeapSize = 256;

        // Create heaps
        m_ResourceHeap = CreateScopedPtr<Heap>();
        m_SamplerHeap = CreateScopedPtr<Heap>();

        // Setup resource heap
        nvrhi::BindlessLayoutDesc desc{
            .visibility = nvrhi::ShaderType::All,
            .firstSlot = 0,
            .maxCapacity = s_MaxSlots,
            .layoutType = nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv,
        };

        m_ResourceHeap->BindingLayout = device->createBindlessLayout(desc);
        m_ResourceHeap->DescriptorTable = device->createDescriptorTable(m_ResourceHeap->BindingLayout);
        m_ResourceHeap->Capacity = initialHeapSize;
        device->resizeDescriptorTable(m_ResourceHeap->DescriptorTable, initialHeapSize, false);

        // Setup sampler heap
        desc.maxCapacity = s_MaxSamplerSlots;
        desc.layoutType = nvrhi::BindlessLayoutDesc::LayoutType::MutableSampler;

        m_SamplerHeap->BindingLayout = device->createBindlessLayout(desc);
        m_SamplerHeap->DescriptorTable = device->createDescriptorTable(m_SamplerHeap->BindingLayout);
        m_SamplerHeap->Capacity = initialHeapSize;
        device->resizeDescriptorTable(m_SamplerHeap->DescriptorTable, initialHeapSize, false);
    }

    auto DescriptorManager::Register(const Ref<Sampler>& resource) -> BindlessHandle
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        const std::scoped_lock lock(m_SamplerHeap->Mutex);

        // Resize if full
        const uint32_t slot = GetNextSlot(device, m_SamplerHeap);
        if (slot == std::numeric_limits<uint32_t>::max())
            return {};

        // Write to table
        const auto item = nvrhi::BindingSetItem::Sampler(slot, resource->GetSampler());
        EP_ASSERT(m_SamplerHeap->DescriptorTable->getCapacity() > slot);
        device->writeDescriptorTable(m_SamplerHeap->DescriptorTable, item);

        return { shared_from_this(), slot, BindlessHeapType::Sampler };
    }

    auto DescriptorManager::GetResourceDT() const -> nvrhi::DescriptorTableHandle
    {
        if (m_ResourceHeap)
            return m_ResourceHeap->DescriptorTable;
        return nullptr;
    }

    auto DescriptorManager::GetSamplerDT() const -> nvrhi::DescriptorTableHandle
    {
        if (m_SamplerHeap)
            return m_SamplerHeap->DescriptorTable;
        return nullptr;
    }

    auto DescriptorManager::Release(const BindlessHeapType type, uint32_t index) const -> void
    {
        const auto& heap = type == BindlessHeapType::Resource ? m_ResourceHeap : m_SamplerHeap;
        const std::scoped_lock lock(heap->Mutex);
        heap->FreeList.emplace_back(index);
    }

    auto DescriptorManager::GetNextSlot(nvrhi::IDevice* device, const ScopedPtr<Heap>& heap) -> uint32_t
    {
        // Reuse slot if possible
        if (!heap->FreeList.empty())
        {
            const uint32_t index = heap->FreeList.back();
            heap->FreeList.pop_back();
            return index;
        }

        if (heap->NextFreeSlot == heap->Capacity)
        {
            // Capacity reached, grow
            auto newSize = static_cast<uint32_t>(heap->Capacity * 1.5);
            const uint32_t maxSize =
                    heap->BindingLayout->getBindlessDesc()->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv
                    ? s_MaxSlots
                    : s_MaxSamplerSlots;

            if (newSize > maxSize)
            {
                Log::Warn("Trying to resize descriptor heap to a size larger than the maximum: {} (Max: {}). Resizing to max size instead.", newSize, maxSize);
                newSize = maxSize;
            }

            if (newSize == maxSize && heap->NextFreeSlot == newSize)
            {
                Log::Warn("Trying to resize descriptor heap to a size that is the same as the current size: {}", newSize);
                return std::numeric_limits<uint32_t>::max();
            }

            device->resizeDescriptorTable(heap->DescriptorTable, newSize, true);
            heap->Capacity = newSize;
        }

        return heap->NextFreeSlot++;
    }
}
