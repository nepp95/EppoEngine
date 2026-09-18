#include "TestSupport/EppoTest.h"

#include <thread>
#include <vector>

using namespace Eppo;

namespace
{
    struct RefSentinel : RefCtr
    {
        bool* Destroyed = nullptr;

        ~RefSentinel() override
        {
            if (Destroyed)
                *Destroyed = true;
        }
    };

    struct RefTestBase : RefCtr
    {
        uint32_t BaseValue = 7;
    };

    struct RefTestDerived : RefTestBase
    {
        uint32_t DerivedValue = 11;
    };
}

TEST(Core, Ref_Create_InitializesSingleReference)
{
    const auto sentinel = Ref<RefSentinel>::Create();

    EXPECT_EQ(1ul, sentinel->GetRefCount());
    EXPECT_TRUE(IsLive(sentinel.Raw()));
}

TEST(Core, Ref_CopyIncrementsReferenceCount)
{
    const auto sentinel = Ref<RefSentinel>::Create();

    {
        const auto copy = sentinel;
        EXPECT_EQ(2ul, sentinel->GetRefCount());
    }

    EXPECT_EQ(1ul, sentinel->GetRefCount());
}

TEST(Core, Ref_MoveTransfersOwnershipWithoutCountChange)
{
    auto sentinel = Ref<RefSentinel>::Create();

    auto moved = std::move(sentinel);

    EXPECT_EQ(1ul, moved->GetRefCount());
    EXPECT_TRUE(sentinel == nullptr);
}

TEST(Core, Ref_LastReleaseDestroysObject)
{
    bool destroyed = false;

    {
        auto sentinel = Ref<RefSentinel>::Create();
        sentinel->Destroyed = &destroyed;
    }

    EXPECT_TRUE(destroyed);
}

TEST(Core, Ref_RawPointerConstructionAdoptsExistingObject)
{
    bool destroyed = false;
    auto sentinel = Ref<RefSentinel>::Create();
    sentinel->Destroyed = &destroyed;

    {
        const auto adopted = Ref<RefSentinel>(sentinel.Raw());
        EXPECT_EQ(2ul, sentinel->GetRefCount());
    }

    EXPECT_EQ(1ul, sentinel->GetRefCount());
    EXPECT_FALSE(destroyed);

    sentinel.Reset();

    EXPECT_TRUE(destroyed);
}

TEST(Core, Ref_SelfAssignmentKeepsObjectAlive)
{
    bool destroyed = false;
    auto sentinel = Ref<RefSentinel>::Create();
    sentinel->Destroyed = &destroyed;

    Ref<RefSentinel>& alias = sentinel;
    alias = alias;

    EXPECT_EQ(1ul, sentinel->GetRefCount());
    EXPECT_FALSE(destroyed);
}

TEST(Core, Ref_NullptrAssignmentDestroysObject)
{
    bool destroyed = false;
    auto sentinel = Ref<RefSentinel>::Create();
    sentinel->Destroyed = &destroyed;

    sentinel = nullptr;

    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(sentinel == nullptr);
}

TEST(Core, Ref_ResetReplacesTargetAndDestroysOld)
{
    bool firstDestroyed = false;
    auto first = Ref<RefSentinel>::Create();
    first->Destroyed = &firstDestroyed;
    const auto second = Ref<RefSentinel>::Create();

    first.Reset(second.Raw());

    EXPECT_TRUE(firstDestroyed);
    EXPECT_EQ(2ul, second->GetRefCount());
    EXPECT_TRUE(first.Raw() == second.Raw());
}

TEST(Core, Ref_AsSharesOwnershipAcrossHierarchy)
{
    const auto derived = Ref<RefTestDerived>::Create();

    const auto base = derived.As<RefTestBase>();

    EXPECT_EQ(2ul, derived->GetRefCount());
    EXPECT_EQ(7u, base->BaseValue);
    EXPECT_TRUE(base.Raw() == static_cast<RefTestBase*>(derived.Raw()));
}

TEST(Core, RefVoid_KeepsResourceAliveAfterCallerDropsTypedRef)
{
    bool destroyed = false;
    auto typed = Ref<RefSentinel>::Create();
    typed->Destroyed = &destroyed;
    void* rawInstance = typed.Raw();

    {
        Ref<void> erased = typed;
        typed.Reset();

        EXPECT_TRUE(IsLive(rawInstance));
    }

    EXPECT_TRUE(destroyed);
#ifdef EP_DEBUG
    EXPECT_FALSE(IsLive(rawInstance));
#endif
}

TEST(Core, Ref_ConcurrentCopiesPreserveReferenceCount)
{
    constexpr uint32_t threadCount = 8;
    constexpr uint32_t copiesPerThread = 1000;

    auto sentinel = Ref<RefSentinel>::Create();

    {
        std::vector<std::thread> threads;
        for (uint32_t thread = 0; thread < threadCount; thread++)
        {
            threads.emplace_back([&sentinel]() -> void
            {
                for (uint32_t copy = 0; copy < copiesPerThread; copy++)
                {
                    const auto local = sentinel;
                    EXPECT_EQ(2ul, local->GetRefCount());
                }
            });
        }

        for (auto& thread : threads)
            thread.join();
    }

    EXPECT_EQ(1ul, sentinel->GetRefCount());
}
