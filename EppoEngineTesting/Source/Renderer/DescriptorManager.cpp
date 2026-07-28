#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Sampler.h"

using namespace Eppo;

SUITE(Renderer)
{
    TEST(DescriptorManager_CreatesBindlessLayouts)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);
        CHECK(resourceHeap->BindingLayout);

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);
        CHECK(samplerHeap->BindingLayout);
    }

    TEST(DescriptorManager_CreatesDescriptorHeaps)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);
        CHECK(resourceHeap->DescriptorTable);
        CHECK_EQUAL(256, resourceHeap->Capacity);

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);
        CHECK(samplerHeap->DescriptorTable);
        CHECK_EQUAL(256, samplerHeap->Capacity);
    }

    TEST(DescriptorManager_RegisterResourceIncreasesNextFreeSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& dm = DeviceManager::Get();
        const auto& image = dm->GetCurrentSwapchainImage().Framebuffer->GetFinalImage();
        REQUIRE CHECK(image);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        CHECK_EQUAL(0, resourceHeap->NextFreeSlot);
        (void)manager->Register(image);
        CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_RegisterSamplerIncreasesNextFreeSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto sampler = Sampler::Create({}, manager);
        REQUIRE CHECK(sampler);

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);

        CHECK_EQUAL(1, samplerHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_RegisterResourceReturnsValidHandle)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& dm = DeviceManager::Get();
        const auto& image = dm->GetCurrentSwapchainImage().Framebuffer->GetFinalImage();
        REQUIRE CHECK(image);

        const auto handle = manager->Register(image);
        CHECK(handle.Index != std::numeric_limits<uint32_t>::max());
        CHECK(handle.HeapType == BindlessHeapType::Resource);
    }

    TEST(DescriptorManager_RegisterSamplerReturnsValidHandle)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto sampler = Sampler::Create({}, manager);
        REQUIRE CHECK(sampler);

        const auto& handle = sampler->GetBindlessHandle();
        CHECK(handle.Index != std::numeric_limits<uint32_t>::max());
        CHECK(handle.Index < 2048);
        CHECK(handle.HeapType == BindlessHeapType::Sampler);
    }

    TEST(DescriptorManager_RegisterResourceIncreasesCapacityIfFull)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        constexpr uint32_t initialSize = 256;
        constexpr uint32_t trySize = initialSize + 1;

        std::array<BindlessHandle, trySize> handles{};
        for (uint32_t i = 0; i < trySize; i++)
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            REQUIRE CHECK(buffer);
            handles.at(i) = manager->Register(buffer);
            CHECK(handles.at(i).Index != std::numeric_limits<uint32_t>::max());
            CHECK(handles.at(i).HeapType == BindlessHeapType::Resource);
        }

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);
        CHECK_EQUAL(384, resourceHeap->Capacity);
        CHECK_EQUAL(trySize, resourceHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_RegisterSamplerIncreasesCapacityIfFull)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        constexpr uint32_t initialSize = 256;
        constexpr uint32_t trySize = initialSize + 1;

        std::vector<Ref<Sampler>> samplers;
        samplers.reserve(trySize);
        for (uint32_t i = 0; i < trySize; i++)
        {
            samplers.emplace_back(Sampler::Create({}, manager));
            REQUIRE CHECK(samplers.back());
            CHECK(samplers.back()->GetBindlessHandle().Index != std::numeric_limits<uint32_t>::max());
            CHECK(samplers.back()->GetBindlessHandle().HeapType == BindlessHeapType::Sampler);
        }

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);
        CHECK_EQUAL(384, samplerHeap->Capacity);
        CHECK_EQUAL(trySize, samplerHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_ReleaseAddsToFreeList)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        uint32_t index = std::numeric_limits<uint32_t>::max();
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            const auto handle = manager->Register(buffer);
            index = handle.Index;
            CHECK(resourceHeap->FreeList.empty());
        }

        CHECK_EQUAL(1, resourceHeap->FreeList.size());
        CHECK_EQUAL(index, resourceHeap->FreeList.back());
    }

    TEST(DescriptorManager_ReleasedSlotIsReused)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        uint32_t freed = std::numeric_limits<uint32_t>::max();
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            const auto handle = manager->Register(buffer);
            freed = handle.Index;
        }

        const auto buffer = CreateRef<UniformBuffer>(256);
        const auto handle = manager->Register(buffer);
        CHECK_EQUAL(freed, handle.Index);
        CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
        CHECK(resourceHeap->FreeList.empty());
    }

    TEST(DescriptorManager_MoveConstructorTransfersOwnership)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        uint32_t index = std::numeric_limits<uint32_t>::max();
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            BindlessHandle handle = manager->Register(buffer);
            index = handle.Index;

            const BindlessHandle moved = std::move(handle);
            CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), handle.Index);
            CHECK(handle.HeapType == BindlessHeapType::Resource);
            CHECK_EQUAL(moved.Index, index);
            CHECK(resourceHeap->FreeList.empty());
        }

        CHECK_EQUAL(1, resourceHeap->FreeList.size());
        CHECK_EQUAL(index, resourceHeap->FreeList.back());
    }

    TEST(DescriptorManager_MoveAssignReleasesTargetSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto bufferA = CreateRef<UniformBuffer>(256);
        const auto bufferB = CreateRef<UniformBuffer>(256);
        BindlessHandle a = manager->Register(bufferA);
        BindlessHandle b = manager->Register(bufferB);
        const uint32_t indexA = a.Index;
        const uint32_t indexB = b.Index;

        a = std::move(b);
        CHECK_EQUAL(1, resourceHeap->FreeList.size());
        CHECK_EQUAL(indexA, resourceHeap->FreeList.back());
        CHECK_EQUAL(indexB, a.Index);
        CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), b.Index);
        CHECK_EQUAL(2, resourceHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_ResourceLayoutIsMutableSrvUavCbv)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto& layout = manager->GetResourceHeap()->BindingLayout;
        REQUIRE CHECK(layout);

        CHECK(layout->getBindlessDesc()->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv);
    }

    TEST(DescriptorManager_SamplerLayoutIsMutableSampler)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);

        const auto& layout = samplerHeap->BindingLayout;
        REQUIRE CHECK(layout);

        CHECK(layout->getBindlessDesc()->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSampler);
    }

    TEST(DescriptorManager_DescriptorTableCapacityMatchesLayoutMaxCapacity)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto& layout = resourceHeap->BindingLayout;
        REQUIRE CHECK(layout);

        CHECK_EQUAL(layout->getBindlessDesc()->maxCapacity, resourceHeap->DescriptorTable->getCapacity());
    }

    TEST(DescriptorManager_HandleReleasesToOwningManager)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        uint32_t index = std::numeric_limits<uint32_t>::max();
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            const auto handle = manager->Register(buffer);
            index = handle.Index;
            CHECK(handle.Manager.lock() == manager);
        }

        CHECK_EQUAL(1, resourceHeap->FreeList.size());
        CHECK_EQUAL(index, resourceHeap->FreeList.back());
    }

    TEST(DescriptorManager_ResourceAndSamplerHeapsAllocateIndependently)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);
        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);

        const auto buffer = CreateRef<UniformBuffer>(256);
        REQUIRE CHECK(buffer);
        (void)manager->Register(buffer);
        CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
        CHECK_EQUAL(0, samplerHeap->NextFreeSlot);

        const auto sampler = Sampler::Create({}, manager);
        REQUIRE CHECK(sampler);
        CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
        CHECK_EQUAL(1, samplerHeap->NextFreeSlot);
    }

    TEST(DescriptorManager_FreeListReuseTakesPriorityOverGrowth)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        std::vector<Ref<UniformBuffer>> buffers;
        std::vector<BindlessHandle> handles;
        for (uint32_t i = 0; i < 256; i++)
        {
            buffers.emplace_back(CreateRef<UniformBuffer>(256));
            handles.emplace_back(manager->Register(buffers.back()));
        }
        CHECK_EQUAL(256, resourceHeap->NextFreeSlot);

        const uint32_t freed = handles.back().Index;
        handles.pop_back();

        // With the heap full, the freed slot must be reused rather than triggering a grow.
        buffers.emplace_back(CreateRef<UniformBuffer>(256));
        const auto reused = manager->Register(buffers.back());
        CHECK_EQUAL(freed, reused.Index);
        CHECK_EQUAL(256, resourceHeap->Capacity);
        CHECK(resourceHeap->FreeList.empty());

        // A following allocation will grow the heap
        buffers.emplace_back(CreateRef<UniformBuffer>(256));
        const auto grown = manager->Register(buffers.back());
        CHECK(resourceHeap->Capacity > 256);
        CHECK_EQUAL(256, grown.Index);
        CHECK(resourceHeap->FreeList.empty());
    }

    TEST(DescriptorManager_RegisterAssignsDistinctSequentialSlots)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        std::array<Ref<UniformBuffer>, 256> buffers{};
        std::array<BindlessHandle, 256> handles{};
        for (uint32_t i = 0; i < 256; i++)
        {
            const auto buffer = CreateRef<UniformBuffer>(256);
            buffers.at(i) = buffer;
            handles.at(i) = manager->Register(buffer);
        }

        for (uint32_t i = 0; i < 256; i++)
            CHECK_EQUAL(i, handles.at(i).Index);
    }

    TEST(DescriptorManager_CapacityGrowsByHalfEachTime)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        std::vector<Ref<UniformBuffer>> buffers;
        std::vector<BindlessHandle> handles;

        CHECK_EQUAL(256, resourceHeap->Capacity);
        for (uint32_t i = 0; i < 256 + 1; i++)
        {
            buffers.emplace_back(CreateRef<UniformBuffer>(256));
            handles.emplace_back(manager->Register(buffers.back()));
        }
        CHECK_EQUAL(384, resourceHeap->Capacity);
    }

    TEST(DescriptorManager_ReleaseDoesNotChangeNextFreeSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto buffer = CreateRef<UniformBuffer>(256);
        CHECK_EQUAL(0, resourceHeap->NextFreeSlot);

        {
            const auto handle = manager->Register(buffer);
            CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
        }

        CHECK_EQUAL(1, resourceHeap->NextFreeSlot);
        CHECK_EQUAL(1, resourceHeap->FreeList.size());
    }

    TEST(DescriptorManager_DefaultHandleIsInvalid)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const BindlessHandle handle;

        CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), handle.Index);
        CHECK(handle.HeapType == BindlessHeapType::Resource);
        CHECK(!handle.Manager.lock());
    }

    TEST(DescriptorManager_DefaultHandleDestructionDoesNotTouchFreeList)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);
        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);

        {
            const BindlessHandle handle;
        }

        CHECK(resourceHeap->FreeList.empty());
        CHECK(samplerHeap->FreeList.empty());
    }

    TEST(DescriptorManager_MoveAssignFromInvalidHandleReleasesTargetSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto buffer = CreateRef<UniformBuffer>(256);
        BindlessHandle a = manager->Register(buffer);
        const uint32_t indexA = a.Index;

        BindlessHandle invalid;
        a = std::move(invalid);

        CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), a.Index);
        CHECK_EQUAL(1, resourceHeap->FreeList.size());
        CHECK_EQUAL(indexA, resourceHeap->FreeList.back());
    }

    TEST(DescriptorManager_MoveAssignIntoInvalidHandleTakesOwnership)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();
        REQUIRE CHECK(resourceHeap);

        const auto buffer = CreateRef<UniformBuffer>(256);
        BindlessHandle a = manager->Register(buffer);
        const uint32_t indexA = a.Index;

        BindlessHandle target;
        target = std::move(a);

        CHECK_EQUAL(indexA, target.Index);
        CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), a.Index);
        CHECK(resourceHeap->FreeList.empty());
    }

    TEST(DescriptorManager_SelfMoveAssignmentKeepsSlot)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& resourceHeap = manager->GetResourceHeap();

        const auto buffer = CreateRef<UniformBuffer>(256);
        BindlessHandle a = manager->Register(buffer);
        const uint32_t indexA = a.Index;

        BindlessHandle& alias = a;
        a = std::move(alias);

        CHECK_EQUAL(indexA, a.Index);
        CHECK(resourceHeap->FreeList.empty());
    }

    TEST(DescriptorManager_SeparateManagersAllocateIndependently)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto managerA = CreateRef<DescriptorManager>();
        REQUIRE CHECK(managerA);
        const auto managerB = CreateRef<DescriptorManager>();
        REQUIRE CHECK(managerB);

        const auto buffer = CreateRef<UniformBuffer>(256);
        (void)managerA->Register(buffer);

        CHECK_EQUAL(1, managerA->GetResourceHeap()->NextFreeSlot);
        CHECK_EQUAL(0, managerB->GetResourceHeap()->NextFreeSlot);
    }

    TEST(DescriptorManager_RegisterOnFullHeapReturnsInvalidHandle)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // The sampler heap's hard cap is 2048; registering past it must fail rather than grow beyond max.
        const auto manager = CreateRef<DescriptorManager>();
        REQUIRE CHECK(manager);

        const auto& samplerHeap = manager->GetSamplerHeap();
        REQUIRE CHECK(samplerHeap);

        std::vector<Ref<Sampler>> samplers;
        for (uint32_t i = 0; i < 2048; i++)
            samplers.emplace_back(Sampler::Create({}, manager));
        CHECK(samplerHeap->NextFreeSlot == 2048);

        const auto sampler = Sampler::Create({}, manager);
        CHECK_EQUAL(std::numeric_limits<uint32_t>::max(), sampler->GetBindlessIndex());
        CHECK(samplerHeap->Capacity <= 2048);
    }
}
