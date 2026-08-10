#include "TestSupport/EppoTest.h"
#include "Core/ThreadPool/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <thread>

using Eppo::TaskFn;
using Eppo::TaskId;
using Eppo::TaskStatus;
using Eppo::ThreadPool;

// Every wait here is deadline-bounded. A wedged pool must fail its test, not hang the
// whole CTest run, so nothing in this file blocks on a condition that may never hold.
namespace
{
    constexpr auto s_WaitTimeout = std::chrono::seconds(5);

    auto WaitUntil(const std::function<bool()>& predicate) -> bool
    {
        const auto deadline = std::chrono::steady_clock::now() + s_WaitTimeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return predicate();
    }

    // Completion callbacks only run inside Flush, so anything waiting on a callback has
    // to keep pumping the pool from the calling (owner) thread.
    auto FlushUntil(ThreadPool& pool, const std::function<bool()>& predicate) -> bool
    {
        const auto deadline = std::chrono::steady_clock::now() + s_WaitTimeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            pool.Flush();
            if (predicate())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        pool.Flush();
        return predicate();
    }

    // Blocks a worker until the gate opens, so a test can hold tasks in a known state.
    // Self-releasing on the same deadline: a failed test must not deadlock shutdown.
    auto WaitForGate(const std::atomic<bool>& gate) -> void
    {
        const auto deadline = std::chrono::steady_clock::now() + s_WaitTimeout;
        while (!gate.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST(Core, ThreadPool_QueueTask_ReturnsNonZeroTaskId)
{
    ThreadPool pool;

    const auto id = pool.QueueTask(
        "Task",
        []() -> void
        {
        },
        nullptr
    );

    EXPECT_NE(0u, id);
}

TEST(Core, ThreadPool_QueueTask_AssignsUniqueTaskIds)
{
    ThreadPool pool;

    const auto first = pool.QueueTask(
        "First",
        []() -> void
        {
        },
        nullptr
    );
    const auto second = pool.QueueTask(
        "Second",
        []() -> void
        {
        },
        nullptr
    );

    EXPECT_NE(first, second);
}

TEST(Core, ThreadPool_Flush_InvokesCompletionOnCallingThread)
{
    const auto callingThread = std::this_thread::get_id();

    std::atomic<bool> invoked = false;
    std::atomic<bool> onCallingThread = false;
    ThreadPool pool;

    pool.QueueTask(
        "Task",
        []() -> void
        {
        },
        [&invoked, &onCallingThread, callingThread](const TaskStatus status) -> void
        {
            onCallingThread.store(std::this_thread::get_id() == callingThread && status == TaskStatus::Completed);
            invoked.store(true);
        }
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&invoked]() -> bool
        {
            return invoked.load();
        }
    ));
    EXPECT_TRUE(onCallingThread.load());
}

TEST(Core, ThreadPool_QueueTask_ThrowingTaskReportsFailedStatus)
{
    std::atomic<bool> invoked = false;
    std::atomic<TaskStatus> reported = TaskStatus::Pending;
    ThreadPool pool;

    pool.QueueTask(
        "Throwing",
        []() -> void
        {
            throw std::runtime_error("expected");
        },
        [&invoked, &reported](const TaskStatus status) -> void
        {
            reported.store(status);
            invoked.store(true);
        }
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&invoked]() -> bool
        {
            return invoked.load();
        }
    ));
    EXPECT_EQ(TaskStatus::Failed, reported.load());
}

TEST(Core, ThreadPool_CancelAll_FiresCompletionsWithCancelledStatus)
{
    constexpr uint32_t taskCount = 500;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> ran = 0;
    std::atomic<uint32_t> cancelled = 0;
    std::atomic<uint32_t> completed = 0;
    ThreadPool pool;

    // The gate holds every worker, so the rest of the tasks are guaranteed to still be
    // queued when CancelAll runs. Without it the pool drains all 500 first and there is
    // nothing left to cancel.
    for (uint32_t i = 0; i < taskCount; i++)
    {
        pool.QueueTask(
            "Task",
            [&gate, &ran]() -> void
            {
                WaitForGate(gate);
                ran++;
            },
            [&cancelled, &completed](const TaskStatus status) -> void
            {
                if (status == TaskStatus::Cancelled)
                    cancelled++;
                else
                    completed++;
            }
        );
    }

    pool.CancelAll();
    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&cancelled, &completed]() -> bool
        {
            return cancelled.load() + completed.load() == taskCount;
        }
    ));

    // Far more tasks than any plausible worker count, so some must still have been queued.
    EXPECT_GT(cancelled.load(), 0u);
    EXPECT_EQ(taskCount, cancelled.load() + completed.load());

    // A cancelled task must never have executed its body.
    EXPECT_EQ(completed.load(), ran.load());
}

TEST(Core, ThreadPool_Shutdown_WithoutCancelDrainsQueuedTasks)
{
    constexpr uint32_t taskCount = 200;

    std::atomic<uint32_t> ran = 0;
    ThreadPool pool;

    for (uint32_t i = 0; i < taskCount; i++)
        pool.QueueTask(
            "Task",
            [&ran]() -> void
            {
                ran++;
            },
            nullptr
        );

    pool.Shutdown(false);

    EXPECT_EQ(taskCount, ran.load());
    EXPECT_EQ(0u, pool.GetPendingTasksCount());
}

TEST(Core, ThreadPool_Shutdown_FlushesCompletionCallbacksBeforeReturning)
{
    std::atomic<uint32_t> invoked = 0;
    ThreadPool pool;

    for (uint32_t i = 0; i < 50; i++)
        pool.QueueTask(
            "Task",
            []() -> void
            {
            },
            [&invoked](TaskStatus) -> void
            {
                invoked++;
            }
        );

    pool.Shutdown(false);

    EXPECT_EQ(50u, invoked.load());
}

TEST(Core, ThreadPool_Flush_ContinuesAfterCompletionCallbackThrows)
{
    std::atomic<bool> subsequentCallbackInvoked = false;
    ThreadPool pool;

    pool.QueueTask(
        "Throwing completion",
        []() -> void
        {
        },
        [](TaskStatus) -> void
        {
            throw std::runtime_error("Completion failed");
        }
    );
    pool.QueueTask(
        "Subsequent completion",
        []() -> void
        {
        },
        [&subsequentCallbackInvoked](TaskStatus) -> void
        {
            subsequentCallbackInvoked.store(true);
        }
    );

    EXPECT_NO_THROW(pool.Shutdown(false));
    EXPECT_TRUE(subsequentCallbackInvoked.load());
}

TEST(Core, ThreadPool_Shutdown_RejectsNewTasks)
{
    std::atomic<bool> ran = false;
    ThreadPool pool;

    pool.Shutdown(true);

    const auto id = pool.QueueTask(
        "Late task",
        [&ran]() -> void
        {
            ran.store(true);
        },
        nullptr
    );

    EXPECT_EQ(0u, id);
    EXPECT_FALSE(ran.load());
    EXPECT_EQ(0u, pool.GetPendingTasksCount());
}

TEST(Core, ThreadPool_GetPendingTasksCount_ReflectsQueuedAndInFlightTasks)
{
    constexpr uint32_t taskCount = 50;

    std::atomic<bool> gate = false;
    ThreadPool pool;

    EXPECT_EQ(0u, pool.GetPendingTasksCount());

    for (uint32_t i = 0; i < taskCount; i++)
        pool.QueueTask(
            "Gated",
            [&gate]() -> void
            {
                WaitForGate(gate);
            },
            nullptr
        );

    // Gated tasks cannot complete, so queued + in-flight stays at the full count.
    EXPECT_EQ(taskCount, pool.GetPendingTasksCount());

    gate.store(true, std::memory_order_release);

    EXPECT_TRUE(WaitUntil(
        [&pool]() -> bool
        {
            return pool.GetPendingTasksCount() == 0;
        }
    ));
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_RunsAfterAllDependenciesComplete)
{
    std::atomic<uint32_t> dependenciesRun = 0;
    std::atomic<uint32_t> countWhenDependentRan = 0;
    std::atomic<bool> dependentRan = false;
    ThreadPool pool;

    std::vector<TaskId> dependencies;
    for (uint32_t i = 0; i < 3; i++)
        dependencies.emplace_back(pool.QueueTask(
            "Dependency",
            [&dependenciesRun]() -> void
            {
                dependenciesRun++;
            },
            nullptr
        ));

    pool.QueueTaskWithDependencies(
        "Dependent",
        [&dependenciesRun, &countWhenDependentRan, &dependentRan]() -> void
        {
            countWhenDependentRan.store(dependenciesRun.load());
            dependentRan.store(true);
        },
        nullptr, dependencies
    );

    ASSERT_TRUE(WaitUntil(
        [&dependentRan]() -> bool
        {
            return dependentRan.load();
        }
    ));
    EXPECT_EQ(3u, countWhenDependentRan.load());
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_FiresCompletionCallback)
{
    std::atomic<bool> invoked = false;
    std::atomic<TaskStatus> reported = TaskStatus::Pending;
    ThreadPool pool;

    const auto dependency = pool.QueueTask(
        "Dependency",
        []() -> void
        {
        },
        nullptr
    );

    pool.QueueTaskWithDependencies(
        "Dependent",
        []() -> void
        {
        },
        [&invoked, &reported](const TaskStatus status) -> void
        {
            reported.store(status);
            invoked.store(true);
        },
        { dependency }
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&invoked]() -> bool
        {
            return invoked.load();
        }
    ));
    EXPECT_EQ(TaskStatus::Completed, reported.load());
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_TreatsCompletedDependencyAsSatisfied)
{
    std::atomic<bool> dependentRan = false;
    ThreadPool pool;

    const auto dependency = pool.QueueTask(
        "Dependency",
        []() -> void
        {
        },
        nullptr
    );

    // Wait for the work to finish without flushing, so the dependency is complete but
    // still present in the pool's task table.
    ASSERT_TRUE(WaitUntil(
        [&pool]() -> bool
        {
            return pool.GetPendingTasksCount() == 0;
        }
    ));

    pool.QueueTaskWithDependencies(
        "Dependent",
        [&dependentRan]() -> void
        {
            dependentRan.store(true);
        },
        nullptr, { dependency }
    );

    EXPECT_TRUE(WaitUntil(
        [&dependentRan]() -> bool
        {
            return dependentRan.load();
        }
    ));
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_TreatsFlushedDependencyAsSatisfied)
{
    std::atomic<bool> dependencyRan = false;
    std::atomic<bool> dependentRan = false;
    ThreadPool pool;

    const auto dependency = pool.QueueTask(
        "Dependency",
        [&dependencyRan]() -> void
        {
            dependencyRan.store(true);
        },
        nullptr
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&dependencyRan]() -> bool
        {
            return dependencyRan.load();
        }
    ));

    pool.QueueTaskWithDependencies(
        "Dependent",
        [&dependentRan]() -> void
        {
            dependentRan.store(true);
        },
        nullptr, { dependency }
    );

    EXPECT_TRUE(WaitUntil(
        [&dependentRan]() -> bool
        {
            return dependentRan.load();
        }
    ));
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_RejectsUnknownDependency)
{
    std::atomic<bool> ran = false;
    ThreadPool pool;

    constexpr TaskId neverIssued = 999999;

    TaskId id = 1;
    EXPECT_NO_THROW(
        id = pool.QueueTaskWithDependencies(
            "Dependent",
            [&ran]() -> void
            {
                ran.store(true);
            },
            nullptr, { neverIssued }
        )
    );

    EXPECT_EQ(0u, id);
    EXPECT_FALSE(ran.load());
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_RejectsUnknownDependencyWithoutCorruptingValidOnes)
{
    std::atomic<bool> gate = false;
    std::atomic<bool> ran = false;
    ThreadPool pool;

    constexpr TaskId neverIssued = 999999;
    const auto valid = pool.QueueTask(
        "Gated",
        [&gate]() -> void
        {
            WaitForGate(gate);
        },
        nullptr
    );

    TaskId id = 1;
    EXPECT_NO_THROW(
        id = pool.QueueTaskWithDependencies(
            "Dependent",
            [&ran]() -> void
            {
                ran.store(true);
            },
            nullptr, { valid, neverIssued }
        )
    );

    EXPECT_EQ(0u, id);

    // The rejected task must not have registered against the valid dependency: releasing
    // that dependency must not fault when the worker walks its dependents.
    gate.store(true, std::memory_order_release);

    EXPECT_TRUE(WaitUntil(
        [&pool]() -> bool
        {
            return pool.GetPendingTasksCount() == 0;
        }
    ));
    EXPECT_FALSE(ran.load());
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_ChainOfThousandTasksCompletesInOrder)
{
    constexpr uint32_t chainLength = 1000;

    std::atomic<uint32_t> nextExpected = 0;
    std::atomic<uint32_t> outOfOrder = 0;
    std::atomic<uint32_t> ran = 0;
    ThreadPool pool;

    const auto step = [&nextExpected, &outOfOrder, &ran](const uint32_t index) -> TaskFn
    {
        return [&nextExpected, &outOfOrder, &ran, index]() -> void
        {
            if (nextExpected.fetch_add(1) != index)
                outOfOrder++;
            ran++;
        };
    };

    auto previous = pool.QueueTask("Chain", step(0), nullptr);
    for (uint32_t i = 1; i < chainLength; i++)
        previous = pool.QueueTaskWithDependencies("Chain", step(i), nullptr, { previous });

    ASSERT_TRUE(WaitUntil(
        [&ran]() -> bool
        {
            return ran.load() == chainLength;
        }
    ));
    EXPECT_EQ(0u, outOfOrder.load());
}
