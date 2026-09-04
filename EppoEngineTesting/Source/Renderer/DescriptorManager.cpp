#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Sampler.h"

using namespace Eppo;

TEST(Renderer, DescriptorManager_CreatesBindlessLayouts)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);
    EXPECT_TRUE(resourceHeap->BindingLayout);

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);
    EXPECT_TRUE(samplerHeap->BindingLayout);
}

TEST(Renderer, DescriptorManager_CreatesDescriptorHeaps)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);
    EXPECT_TRUE(resourceHeap->DescriptorTable);
    EXPECT_EQ(256, resourceHeap->Capacity);

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);
    EXPECT_TRUE(samplerHeap->DescriptorTable);
    EXPECT_EQ(256, samplerHeap->Capacity);
}

TEST(Renderer, DescriptorManager_RegisterResourceIncreasesNextFreeSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto image = Image::Create(ImageSpecification{
        .ImageFormat = nvrhi::Format::RGBA8_UNORM, .Width = 1, .Height = 1,
    });
    EP_REQUIRE(image);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    EXPECT_EQ(0, resourceHeap->NextFreeSlot);
    (void)manager->Register(image);
    EXPECT_EQ(1, resourceHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_RegisterSamplerIncreasesNextFreeSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto sampler = Sampler::Create({}, manager);
    EP_REQUIRE(sampler);

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);

    EXPECT_EQ(1, samplerHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_RegisterResourceReturnsValidHandle)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto image = Image::Create(ImageSpecification{
        .ImageFormat = nvrhi::Format::RGBA8_UNORM, .Width = 1, .Height = 1,
    });
    EP_REQUIRE(image);

    const auto handle = manager->Register(image);
    EXPECT_TRUE(handle.Index != std::numeric_limits<uint32_t>::max());
    EXPECT_TRUE(handle.HeapType == BindlessHeapType::Resource);
}

TEST(Renderer, DescriptorManager_RegisterSamplerReturnsValidHandle)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto sampler = Sampler::Create({}, manager);
    EP_REQUIRE(sampler);

    const auto& handle = sampler->GetBindlessHandle();
    EXPECT_TRUE(handle.Index != std::numeric_limits<uint32_t>::max());
    EXPECT_TRUE(handle.Index < 2048);
    EXPECT_TRUE(handle.HeapType == BindlessHeapType::Sampler);
}

TEST(Renderer, DescriptorManager_RegisterResourceIncreasesCapacityIfFull)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    constexpr uint32_t initialSize = 256;
    constexpr uint32_t trySize = initialSize + 1;

    std::array<BindlessHandle, trySize> handles{};
    for (uint32_t i = 0; i < trySize; i++)
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        EP_REQUIRE(buffer);
        handles.at(i) = manager->Register(buffer);
        EXPECT_TRUE(handles.at(i).Index != std::numeric_limits<uint32_t>::max());
        EXPECT_TRUE(handles.at(i).HeapType == BindlessHeapType::Resource);
    }

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);
    EXPECT_EQ(384, resourceHeap->Capacity);
    EXPECT_EQ(trySize, resourceHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_RegisterSamplerIncreasesCapacityIfFull)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    constexpr uint32_t initialSize = 256;
    constexpr uint32_t trySize = initialSize + 1;

    std::vector<Ref<Sampler>> samplers;
    samplers.reserve(trySize);
    for (uint32_t i = 0; i < trySize; i++)
    {
        samplers.emplace_back(Sampler::Create({}, manager));
        EP_REQUIRE(samplers.back());
        EXPECT_TRUE(samplers.back()->GetBindlessHandle().Index != std::numeric_limits<uint32_t>::max());
        EXPECT_TRUE(samplers.back()->GetBindlessHandle().HeapType == BindlessHeapType::Sampler);
    }

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);
    EXPECT_EQ(384, samplerHeap->Capacity);
    EXPECT_EQ(trySize, samplerHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_ReleaseAddsToFreeList)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    uint32_t index = std::numeric_limits<uint32_t>::max();
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        const auto handle = manager->Register(buffer);
        index = handle.Index;
        EXPECT_TRUE(resourceHeap->FreeList.empty());
    }

    EXPECT_EQ(1, resourceHeap->FreeList.size());
    EXPECT_EQ(index, resourceHeap->FreeList.back());
}

TEST(Renderer, DescriptorManager_ReleasedSlotIsReused)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    uint32_t freed = std::numeric_limits<uint32_t>::max();
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        const auto handle = manager->Register(buffer);
        freed = handle.Index;
    }

    const auto buffer = CreateRef<UniformBuffer>(256);
    const auto handle = manager->Register(buffer);
    EXPECT_EQ(freed, handle.Index);
    EXPECT_EQ(1, resourceHeap->NextFreeSlot);
    EXPECT_TRUE(resourceHeap->FreeList.empty());
}

TEST(Renderer, DescriptorManager_MoveConstructorTransfersOwnership)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    uint32_t index = std::numeric_limits<uint32_t>::max();
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        BindlessHandle handle = manager->Register(buffer);
        index = handle.Index;

        const BindlessHandle moved = std::move(handle);
        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), handle.Index);
        EXPECT_TRUE(handle.HeapType == BindlessHeapType::Resource);
        EXPECT_EQ(moved.Index, index);
        EXPECT_TRUE(resourceHeap->FreeList.empty());
    }

    EXPECT_EQ(1, resourceHeap->FreeList.size());
    EXPECT_EQ(index, resourceHeap->FreeList.back());
}

TEST(Renderer, DescriptorManager_MoveAssignReleasesTargetSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto bufferA = CreateRef<UniformBuffer>(256);
    const auto bufferB = CreateRef<UniformBuffer>(256);
    BindlessHandle a = manager->Register(bufferA);
    BindlessHandle b = manager->Register(bufferB);
    const uint32_t indexA = a.Index;
    const uint32_t indexB = b.Index;

    a = std::move(b);
    EXPECT_EQ(1, resourceHeap->FreeList.size());
    EXPECT_EQ(indexA, resourceHeap->FreeList.back());
    EXPECT_EQ(indexB, a.Index);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), b.Index);
    EXPECT_EQ(2, resourceHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_ResourceLayoutIsMutableSrvUavCbv)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto& layout = manager->GetResourceHeap()->BindingLayout;
    EP_REQUIRE(layout);

    EXPECT_TRUE(layout->getBindlessDesc()->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv);
}

TEST(Renderer, DescriptorManager_SamplerLayoutIsMutableSampler)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);

    const auto& layout = samplerHeap->BindingLayout;
    EP_REQUIRE(layout);

    EXPECT_TRUE(layout->getBindlessDesc()->layoutType == nvrhi::BindlessLayoutDesc::LayoutType::MutableSampler);
}

TEST(Renderer, DescriptorManager_DescriptorTableCapacityCoversAllocatedSlots)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto& layout = resourceHeap->BindingLayout;
    EP_REQUIRE(layout);

    EXPECT_GE(resourceHeap->DescriptorTable->getCapacity(), resourceHeap->Capacity);
    EXPECT_LE(resourceHeap->DescriptorTable->getCapacity(), layout->getBindlessDesc()->maxCapacity);
}

TEST(Renderer, DescriptorManager_HandleReleasesToOwningManager)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    uint32_t index = std::numeric_limits<uint32_t>::max();
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        const auto handle = manager->Register(buffer);
        index = handle.Index;
        EXPECT_TRUE(handle.Manager.lock() == manager);
    }

    EXPECT_EQ(1, resourceHeap->FreeList.size());
    EXPECT_EQ(index, resourceHeap->FreeList.back());
}

TEST(Renderer, DescriptorManager_ResourceAndSamplerHeapsAllocateIndependently)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);
    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);

    const auto buffer = CreateRef<UniformBuffer>(256);
    EP_REQUIRE(buffer);
    (void)manager->Register(buffer);
    EXPECT_EQ(1, resourceHeap->NextFreeSlot);
    EXPECT_EQ(0, samplerHeap->NextFreeSlot);

    const auto sampler = Sampler::Create({}, manager);
    EP_REQUIRE(sampler);
    EXPECT_EQ(1, resourceHeap->NextFreeSlot);
    EXPECT_EQ(1, samplerHeap->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_FreeListReuseTakesPriorityOverGrowth)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    std::vector<Ref<UniformBuffer>> buffers;
    std::vector<BindlessHandle> handles;
    for (uint32_t i = 0; i < 256; i++)
    {
        buffers.emplace_back(CreateRef<UniformBuffer>(256));
        handles.emplace_back(manager->Register(buffers.back()));
    }
    EXPECT_EQ(256, resourceHeap->NextFreeSlot);

    const uint32_t freed = handles.back().Index;
    handles.pop_back();

    // With the heap full, the freed slot must be reused rather than triggering a grow.
    buffers.emplace_back(CreateRef<UniformBuffer>(256));
    const auto reused = manager->Register(buffers.back());
    EXPECT_EQ(freed, reused.Index);
    EXPECT_EQ(256, resourceHeap->Capacity);
    EXPECT_TRUE(resourceHeap->FreeList.empty());

    // A following allocation will grow the heap
    buffers.emplace_back(CreateRef<UniformBuffer>(256));
    const auto grown = manager->Register(buffers.back());
    EXPECT_TRUE(resourceHeap->Capacity > 256);
    EXPECT_EQ(256, grown.Index);
    EXPECT_TRUE(resourceHeap->FreeList.empty());
}

TEST(Renderer, DescriptorManager_RegisterAssignsDistinctSequentialSlots)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    std::array<Ref<UniformBuffer>, 256> buffers{};
    std::array<BindlessHandle, 256> handles{};
    for (uint32_t i = 0; i < 256; i++)
    {
        const auto buffer = CreateRef<UniformBuffer>(256);
        buffers.at(i) = buffer;
        handles.at(i) = manager->Register(buffer);
    }

    for (uint32_t i = 0; i < 256; i++)
        EXPECT_EQ(i, handles.at(i).Index);
}

TEST(Renderer, DescriptorManager_CapacityGrowsByHalfEachTime)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    std::vector<Ref<UniformBuffer>> buffers;
    std::vector<BindlessHandle> handles;

    EXPECT_EQ(256, resourceHeap->Capacity);
    for (uint32_t i = 0; i < 256 + 1; i++)
    {
        buffers.emplace_back(CreateRef<UniformBuffer>(256));
        handles.emplace_back(manager->Register(buffers.back()));
    }
    EXPECT_EQ(384, resourceHeap->Capacity);
}

TEST(Renderer, DescriptorManager_ReleaseDoesNotChangeNextFreeSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto buffer = CreateRef<UniformBuffer>(256);
    EXPECT_EQ(0, resourceHeap->NextFreeSlot);

    {
        const auto handle = manager->Register(buffer);
        EXPECT_EQ(1, resourceHeap->NextFreeSlot);
    }

    EXPECT_EQ(1, resourceHeap->NextFreeSlot);
    EXPECT_EQ(1, resourceHeap->FreeList.size());
}

TEST(Renderer, DescriptorManager_DefaultHandleIsInvalid)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const BindlessHandle handle;

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), handle.Index);
    EXPECT_TRUE(handle.HeapType == BindlessHeapType::Resource);
    EXPECT_TRUE(!handle.Manager.lock());
}

TEST(Renderer, DescriptorManager_DefaultHandleDestructionDoesNotTouchFreeList)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);
    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);

    {
        const BindlessHandle handle;
    }

    EXPECT_TRUE(resourceHeap->FreeList.empty());
    EXPECT_TRUE(samplerHeap->FreeList.empty());
}

TEST(Renderer, DescriptorManager_MoveAssignFromInvalidHandleReleasesTargetSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto buffer = CreateRef<UniformBuffer>(256);
    BindlessHandle a = manager->Register(buffer);
    const uint32_t indexA = a.Index;

    BindlessHandle invalid;
    a = std::move(invalid);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), a.Index);
    EXPECT_EQ(1, resourceHeap->FreeList.size());
    EXPECT_EQ(indexA, resourceHeap->FreeList.back());
}

TEST(Renderer, DescriptorManager_MoveAssignIntoInvalidHandleTakesOwnership)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();
    EP_REQUIRE(resourceHeap);

    const auto buffer = CreateRef<UniformBuffer>(256);
    BindlessHandle a = manager->Register(buffer);
    const uint32_t indexA = a.Index;

    BindlessHandle target;
    target = std::move(a);

    EXPECT_EQ(indexA, target.Index);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), a.Index);
    EXPECT_TRUE(resourceHeap->FreeList.empty());
}

TEST(Renderer, DescriptorManager_SelfMoveAssignmentKeepsSlot)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& resourceHeap = manager->GetResourceHeap();

    const auto buffer = CreateRef<UniformBuffer>(256);
    BindlessHandle a = manager->Register(buffer);
    const uint32_t indexA = a.Index;

    BindlessHandle& alias = a;
    a = std::move(alias);

    EXPECT_EQ(indexA, a.Index);
    EXPECT_TRUE(resourceHeap->FreeList.empty());
}

TEST(Renderer, DescriptorManager_SeparateManagersAllocateIndependently)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto managerA = CreateRef<DescriptorManager>();
    EP_REQUIRE(managerA);
    const auto managerB = CreateRef<DescriptorManager>();
    EP_REQUIRE(managerB);

    const auto buffer = CreateRef<UniformBuffer>(256);
    (void)managerA->Register(buffer);

    EXPECT_EQ(1, managerA->GetResourceHeap()->NextFreeSlot);
    EXPECT_EQ(0, managerB->GetResourceHeap()->NextFreeSlot);
}

TEST(Renderer, DescriptorManager_RegisterOnFullHeapReturnsInvalidHandle)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    // The sampler heap's hard cap is 2048; registering past it must fail rather than grow beyond max.
    const auto manager = CreateRef<DescriptorManager>();
    EP_REQUIRE(manager);

    const auto& samplerHeap = manager->GetSamplerHeap();
    EP_REQUIRE(samplerHeap);

    std::vector<Ref<Sampler>> samplers;
    for (uint32_t i = 0; i < 2048; i++)
        samplers.emplace_back(Sampler::Create({}, manager));
    EXPECT_TRUE(samplerHeap->NextFreeSlot == 2048);

    const auto sampler = Sampler::Create({}, manager);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), sampler->GetBindlessIndex());
    EXPECT_TRUE(samplerHeap->Capacity <= 2048);
}
