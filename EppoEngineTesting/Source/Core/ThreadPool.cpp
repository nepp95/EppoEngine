#include "TestSupport/EppoTest.h"
#include "Core/Threading/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using Eppo::TaskFn;
using Eppo::TaskId;
using Eppo::TaskResult;
using Eppo::TaskStatus;
using Eppo::ThreadPool;

// Every wait here is deadline-bounded. A wedged pool must fail its test, not hang the
// whole CTest run, so nothing in this file blocks on a condition that may never hold.
namespace
{
    constexpr auto s_WaitTimeout = std::chrono::seconds(1);

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

    [[nodiscard]] auto GetThreadPoolWorkerCount() -> uint32_t
    {
        const uint32_t hardwareThreadCount = std::thread::hardware_concurrency();
        return hardwareThreadCount > 2 ? hardwareThreadCount - 2 : 1;
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

TEST(Core, ThreadPool_QueueTaskWithDependencies_FiresDependencyCompletionFirst)
{
    constexpr uint32_t taskCount = 512;

    std::vector<uint8_t> completionStates(taskCount, 0);
    uint32_t outOfOrder = 0;
    ThreadPool pool;

    for (uint32_t i = 0; i < taskCount; i++)
    {
        const auto dependency = pool.QueueTask(
            "Dependency",
            []() -> void
            {
            },
            [&completionStates, &outOfOrder, i](TaskStatus) -> void
            {
                if (completionStates[i] != 0)
                    outOfOrder++;
                completionStates[i] = 1;
            }
        );

        pool.QueueTaskWithDependencies(
            "Dependent",
            []() -> void
            {
            },
            [&completionStates, &outOfOrder, i](TaskStatus) -> void
            {
                if (completionStates[i] != 1)
                    outOfOrder++;
                completionStates[i] = 2;
            },
            { dependency }
        );
    }

    pool.Shutdown(false);

    EXPECT_EQ(0u, outOfOrder);
    for (const auto state : completionStates)
        EXPECT_EQ(2u, state);
}

TEST(Core, ThreadPool_GetTaskGroupSnapshots_IncludesDependencies)
{
    constexpr uint32_t dependencyCount = 2;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> dependenciesRunning = 0;
    ThreadPool pool;

    std::vector<TaskId> dependencies;
    dependencies.reserve(dependencyCount);
    for (uint32_t i = 0; i < dependencyCount; i++)
    {
        dependencies.emplace_back(pool.QueueTask(
            [&gate, &dependenciesRunning]() -> void
            {
                dependenciesRunning.fetch_add(1, std::memory_order_release);
                WaitForGate(gate);
            },
            nullptr
        ));
    }

    pool.QueueTaskWithDependencies("Mesh", []() -> void {}, nullptr, dependencies);

    ASSERT_TRUE(WaitUntil(
        [&dependenciesRunning]() -> bool
        {
            return dependenciesRunning.load(std::memory_order_acquire) == dependencyCount;
        }
    ));

    const auto running = pool.GetTaskGroupSnapshots().at("Mesh");
    EXPECT_EQ(dependencyCount + 1, running.Total.load(std::memory_order_relaxed));
    EXPECT_EQ(dependencyCount, running.Running.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, running.Pending.load(std::memory_order_relaxed));

    gate.store(true, std::memory_order_release);
    pool.Shutdown(false);

    const auto completed = pool.GetTaskGroupSnapshots().at("Mesh");
    EXPECT_EQ(dependencyCount + 1, completed.Total.load(std::memory_order_relaxed));
    EXPECT_EQ(dependencyCount + 1, completed.Completed.load(std::memory_order_relaxed));
    EXPECT_TRUE(completed.IsFinished());
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

// CancelTask must flip a pending task to Cancelled so its body never runs and its
// completion fires with Cancelled status. Filling all workers with gated tasks
// guarantees the target is still queued when CancelTask runs.
TEST(Core, ThreadPool_CancelTask_CancelsPendingTask)
{
    const auto workerCount = GetThreadPoolWorkerCount();
    const auto taskCount = workerCount + 1;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> running = 0;
    std::atomic<bool> targetBodyRan = false;
    std::atomic<TaskStatus> targetStatus = TaskStatus::Pending;
    std::atomic<bool> targetCompletionFired = false;
    std::atomic<uint32_t> otherCompleted = 0;
    ThreadPool pool;

    std::vector<TaskId> ids;
    ids.reserve(taskCount);

    for (uint32_t i = 0; i < taskCount; i++)
    {
        ids.emplace_back(pool.QueueTask(
            "Gated",
            [&gate, &running, &targetBodyRan, i, taskCount]() -> void
            {
                running.fetch_add(1, std::memory_order_release);
                if (i == taskCount - 1)
                    targetBodyRan.store(true);
                WaitForGate(gate);
            },
            [&targetCompletionFired, &targetStatus, &otherCompleted, i, taskCount](const TaskStatus status) -> void
            {
                if (i == taskCount - 1)
                {
                    targetStatus.store(status);
                    targetCompletionFired.store(true);
                }
                else if (status == TaskStatus::Completed)
                {
                    otherCompleted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        ));
    }

    ASSERT_TRUE(WaitUntil(
        [&running, &workerCount]() -> bool
        {
            return running.load(std::memory_order_acquire) >= workerCount;
        }
    ));

    EXPECT_TRUE(pool.CancelTask(ids.back()));

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&targetCompletionFired, &otherCompleted, &workerCount]() -> bool
        {
            return targetCompletionFired.load() && otherCompleted.load() == workerCount;
        }
    ));

    EXPECT_EQ(TaskStatus::Cancelled, targetStatus.load());
    EXPECT_FALSE(targetBodyRan.load());
    EXPECT_EQ(workerCount, otherCompleted.load());
}

// A task that has already been claimed by a worker cannot be cancelled.
TEST(Core, ThreadPool_CancelTask_IgnoresRunningTask)
{
    std::atomic<bool> started = false;
    std::atomic<bool> gate = false;
    std::atomic<bool> invoked = false;
    std::atomic<TaskStatus> reported = TaskStatus::Pending;
    ThreadPool pool;

    const auto id = pool.QueueTask(
        "Gated",
        [&started, &gate]() -> void
        {
            started.store(true, std::memory_order_release);
            WaitForGate(gate);
        },
        [&invoked, &reported](const TaskStatus status) -> void
        {
            reported.store(status);
            invoked.store(true);
        }
    );

    ASSERT_TRUE(WaitUntil(
        [&started]() -> bool
        {
            return started.load(std::memory_order_acquire);
        }
    ));

    EXPECT_FALSE(pool.CancelTask(id));

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&invoked]() -> bool
        {
            return invoked.load();
        }
    ));
    EXPECT_EQ(TaskStatus::Completed, reported.load());
}

// CancelTask on a task that already completed is a no-op.
TEST(Core, ThreadPool_CancelTask_ReturnsFalseForCompletedTask)
{
    std::atomic<bool> invoked = false;
    ThreadPool pool;

    const auto id = pool.QueueTask(
        "Done",
        []() -> void
        {
        },
        [&invoked](TaskStatus) -> void
        {
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

    EXPECT_FALSE(pool.CancelTask(id));
}

// CancelTask on an unknown ID must not crash or hang.
TEST(Core, ThreadPool_CancelTask_ReturnsFalseForUnknownId)
{
    ThreadPool pool;

    EXPECT_FALSE(pool.CancelTask(999999));
}

// ---------------------------------------------------------------------------
// CancelTask: snapshot integration
// ---------------------------------------------------------------------------

// Cancelling a pending task must update the group snapshot: Pending decrements and
// Cancelled increments. The snapshot is the editor's progress UI source of truth.
TEST(Core, ThreadPool_CancelTask_UpdatesGroupSnapshot)
{
    const auto workerCount = GetThreadPoolWorkerCount();
    const auto taskCount = workerCount + 2;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> running = 0;
    std::atomic<uint32_t> completions = 0;
    ThreadPool pool;

    std::vector<TaskId> ids;
    ids.reserve(taskCount);

    for (uint32_t i = 0; i < taskCount; i++)
    {
        ids.emplace_back(pool.QueueTask(
            "SnapshotProbe",
            [&gate, &running]() -> void
            {
                running.fetch_add(1, std::memory_order_release);
                WaitForGate(gate);
            },
            [&completions](TaskStatus) -> void
            {
                completions.fetch_add(1, std::memory_order_relaxed);
            }
        ));
    }

    ASSERT_TRUE(WaitUntil(
        [&running, &workerCount]() -> bool
        {
            return running.load(std::memory_order_acquire) >= workerCount;
        }
    ));

    const auto before = pool.GetTaskGroupSnapshots().at("SnapshotProbe");
    EXPECT_EQ(taskCount, before.Total.load(std::memory_order_relaxed));
    EXPECT_EQ(workerCount, before.Running.load(std::memory_order_relaxed));
    EXPECT_EQ(taskCount - workerCount, before.Pending.load(std::memory_order_relaxed));

    EXPECT_TRUE(pool.CancelTask(ids[workerCount]));
    EXPECT_TRUE(pool.CancelTask(ids[workerCount + 1]));

    const auto afterCancel = pool.GetTaskGroupSnapshots().at("SnapshotProbe");
    EXPECT_EQ(2u, afterCancel.Cancelled.load(std::memory_order_relaxed));
    EXPECT_EQ(0u, afterCancel.Pending.load(std::memory_order_relaxed));

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&completions, &taskCount]() -> bool
        {
            return completions.load(std::memory_order_relaxed) == taskCount;
        }
    ));

    const auto afterFlush = pool.GetTaskGroupSnapshots().at("SnapshotProbe");
    EXPECT_EQ(workerCount, afterFlush.Completed.load(std::memory_order_relaxed));
    EXPECT_EQ(2u, afterFlush.Cancelled.load(std::memory_order_relaxed));
    EXPECT_EQ(taskCount, afterFlush.Total.load(std::memory_order_relaxed));
    EXPECT_TRUE(afterFlush.IsFinished());
}

// Cancelling a task must decrement GetPendingTasksCount so the StatusBar busy
// signal clears when the last task is cancelled, not when a worker picks it up.
TEST(Core, ThreadPool_CancelTask_DecrementsPendingCount)
{
    const auto workerCount = GetThreadPoolWorkerCount();
    const auto taskCount = workerCount + 1;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> running = 0;
    ThreadPool pool;

    std::vector<TaskId> ids;
    ids.reserve(taskCount);

    for (uint32_t i = 0; i < taskCount; i++)
    {
        ids.emplace_back(pool.QueueTask(
            "CountProbe",
            [&gate, &running]() -> void
            {
                running.fetch_add(1, std::memory_order_release);
                WaitForGate(gate);
            },
            nullptr
        ));
    }

    ASSERT_TRUE(WaitUntil(
        [&running, &workerCount]() -> bool
        {
            return running.load(std::memory_order_acquire) >= workerCount;
        }
    ));

    const auto countBefore = pool.GetPendingTasksCount();
    EXPECT_EQ(taskCount, countBefore);

    EXPECT_TRUE(pool.CancelTask(ids.back()));

    EXPECT_EQ(countBefore - 1, pool.GetPendingTasksCount());

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(WaitUntil(
        [&pool]() -> bool
        {
            return pool.GetPendingTasksCount() == 0;
        }
    ));
}

// ---------------------------------------------------------------------------
// CancelTask: idempotency and terminal states
// ---------------------------------------------------------------------------

// Cancelling an already-cancelled task is a no-op, not a double-cancel.
TEST(Core, ThreadPool_CancelTask_AlreadyCancelledReturnsFalse)
{
    const auto workerCount = GetThreadPoolWorkerCount();
    const auto taskCount = workerCount + 1;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> running = 0;
    std::atomic<uint32_t> cancelledCompletions = 0;
    ThreadPool pool;

    std::vector<TaskId> ids;
    ids.reserve(taskCount);

    for (uint32_t i = 0; i < taskCount; i++)
    {
        ids.emplace_back(pool.QueueTask(
            "IdempotencyProbe",
            [&gate, &running]() -> void
            {
                running.fetch_add(1, std::memory_order_release);
                WaitForGate(gate);
            },
            [&cancelledCompletions](const TaskStatus status) -> void
            {
                if (status == TaskStatus::Cancelled)
                    cancelledCompletions.fetch_add(1, std::memory_order_relaxed);
            }
        ));
    }

    ASSERT_TRUE(WaitUntil(
        [&running, &workerCount]() -> bool
        {
            return running.load(std::memory_order_acquire) >= workerCount;
        }
    ));

    EXPECT_TRUE(pool.CancelTask(ids.back()));
    EXPECT_FALSE(pool.CancelTask(ids.back()));

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&cancelledCompletions]() -> bool
        {
            return cancelledCompletions.load() == 1;
        }
    ));

    EXPECT_EQ(1u, cancelledCompletions.load());
}

// Cancelling a failed task returns false — the task already ran and threw.
TEST(Core, ThreadPool_CancelTask_FailedTaskReturnsFalse)
{
    std::atomic<bool> invoked = false;
    ThreadPool pool;

    const auto id = pool.QueueTask(
        "Throwing",
        []() -> void
        {
            throw std::runtime_error("expected");
        },
        [&invoked](TaskStatus) -> void
        {
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

    EXPECT_FALSE(pool.CancelTask(id));
}

// ---------------------------------------------------------------------------
// CancelTask: dependency chain interaction
// ---------------------------------------------------------------------------

// Cancelling a dependency must still resolve its dependents so they don't hang.
// The dependent should run (or be cancellable separately) — it must not deadlock.
TEST(Core, ThreadPool_CancelTask_DependentStillResolvesAfterDependencyCancelled)
{
    const auto workerCount = GetThreadPoolWorkerCount();
    const auto fillerCount = workerCount;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> running = 0;
    std::atomic<bool> dependentRan = false;
    std::atomic<bool> dependencyCompletionFired = false;
    std::atomic<TaskStatus> dependencyStatus = TaskStatus::Pending;
    ThreadPool pool;

    // Fill all workers so the dependency stays pending.
    for (uint32_t i = 0; i < fillerCount; i++)
    {
        pool.QueueTask(
            "Filler",
            [&gate, &running]() -> void
            {
                running.fetch_add(1, std::memory_order_release);
                WaitForGate(gate);
            },
            nullptr
        );
    }

    ASSERT_TRUE(WaitUntil(
        [&running, &workerCount]() -> bool
        {
            return running.load(std::memory_order_acquire) >= workerCount;
        }
    ));

    // Queue a dependency that will be cancelled while pending.
    const auto depId = pool.QueueTask(
        "Dependency",
        []() -> void
        {
        },
        [&dependencyCompletionFired, &dependencyStatus](const TaskStatus status) -> void
        {
            dependencyStatus.store(status);
            dependencyCompletionFired.store(true);
        }
    );

    // Queue a dependent on it.
    pool.QueueTaskWithDependencies(
        "Dependent",
        [&dependentRan]() -> void
        {
            dependentRan.store(true);
        },
        nullptr, { depId }
    );

    // Cancel the dependency while it's still pending.
    EXPECT_TRUE(pool.CancelTask(depId));

    // The cancelled dependency's completion fires, and the dependent is released.
    ASSERT_TRUE(FlushUntil(
        pool,
        [&dependencyCompletionFired]() -> bool
        {
            return dependencyCompletionFired.load();
        }
    ));
    EXPECT_EQ(TaskStatus::Cancelled, dependencyStatus.load());

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(WaitUntil(
        [&dependentRan]() -> bool
        {
            return dependentRan.load();
        }
    ));
    EXPECT_TRUE(dependentRan.load());
}

TEST(Core, ThreadPool_TaskResult_IsEmptyUntilWorkerProducesPayload)
{
    const TaskResult<int32_t> result;

    EXPECT_FALSE(result.has_value());
}

TEST(Core, ThreadPool_TaskResult_HandsWorkerPayloadToCompletion)
{
    struct Payload
    {
        int32_t Value = 0;
        std::string Text;
    };

    const auto result = Eppo::CreateRef<TaskResult<Payload>>();
    std::atomic<bool> completionFired = false;
    TaskStatus reported = TaskStatus::Pending;
    Payload captured;
    ThreadPool pool;

    pool.QueueTask(
        "StructProbe",
        [result]() -> void
        {
            result->emplace(Payload{ .Value = 42, .Text = "worker payload" });
        },
        [&completionFired, &reported, &captured, result](const TaskStatus status) -> void
        {
            reported = status;
            if (result->has_value())
                captured = result->value();
            completionFired.store(true);
        }
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&completionFired]() -> bool
        {
            return completionFired.load();
        }
    ));

    EXPECT_EQ(TaskStatus::Completed, reported);
    EXPECT_EQ(42, captured.Value);
    EXPECT_EQ("worker payload", captured.Text);
}

TEST(Core, ThreadPool_TaskResult_DoesNotEncodeTaskOutcome)
{
    const auto result = Eppo::CreateRef<TaskResult<std::string>>();
    std::atomic<bool> completionFired = false;
    TaskStatus reported = TaskStatus::Pending;
    ThreadPool pool;

    pool.QueueTask(
        "FailureProbe",
        [result]() -> void
        {
            result->emplace("diagnostic payload");
            throw std::runtime_error("expected");
        },
        [&completionFired, &reported](const TaskStatus status) -> void
        {
            reported = status;
            completionFired.store(true);
        }
    );

    ASSERT_TRUE(FlushUntil(
        pool,
        [&completionFired]() -> bool
        {
            return completionFired.load();
        }
    ));

    ASSERT_TRUE(result->has_value());
    EXPECT_EQ("diagnostic payload", result->value());
    EXPECT_EQ(TaskStatus::Failed, reported);
}

TEST(Core, ThreadPool_GetTaskGroupSnapshots_RemainsCoherentDuringConcurrentTransitions)
{
    constexpr uint32_t taskCount = 512;

    std::atomic<bool> gate = false;
    std::atomic<uint32_t> completions = 0;
    ThreadPool pool;

    for (uint32_t i = 0; i < taskCount; i++)
    {
        pool.QueueTask(
            "CoherentSnapshot",
            [&gate]() -> void
            {
                WaitForGate(gate);
            },
            [&completions](TaskStatus) -> void
            {
                completions.fetch_add(1, std::memory_order_relaxed);
            }
        );
    }

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&pool, &completions]() -> bool
        {
            const auto snapshot = pool.GetTaskGroupSnapshots().at("CoherentSnapshot");
            const auto accounted = snapshot.Pending.load(std::memory_order_relaxed) + snapshot.Running.load(std::memory_order_relaxed) +
                snapshot.Completed.load(std::memory_order_relaxed) + snapshot.Failed.load(std::memory_order_relaxed) +
                snapshot.Cancelled.load(std::memory_order_relaxed);
            EXPECT_EQ(snapshot.Total.load(std::memory_order_relaxed), accounted);
            return completions.load(std::memory_order_relaxed) == taskCount;
        }
    ));
}

TEST(Core, ThreadPool_QueueTaskWithDependencies_PublishesAllDependencyWrites)
{
    constexpr uint32_t dependencyCount = 64;

    std::array<uint32_t, dependencyCount> values{};
    std::atomic<bool> gate = false;
    std::atomic<bool> completionFired = false;
    bool observedAllWrites = false;
    ThreadPool pool;

    std::vector<TaskId> dependencies;
    dependencies.reserve(dependencyCount);
    for (uint32_t i = 0; i < dependencyCount; i++)
    {
        dependencies.emplace_back(pool.QueueTask(
            [&gate, &values, i]() -> void
            {
                WaitForGate(gate);
                values[i] = i + 1;
            },
            nullptr
        ));
    }

    pool.QueueTaskWithDependencies(
        [&values, &observedAllWrites]() -> void
        {
            observedAllWrites = true;
            for (uint32_t i = 0; i < values.size(); i++)
                observedAllWrites &= values[i] == i + 1;
        },
        [&completionFired](TaskStatus) -> void
        {
            completionFired.store(true);
        },
        dependencies
    );

    gate.store(true, std::memory_order_release);

    ASSERT_TRUE(FlushUntil(
        pool,
        [&completionFired]() -> bool
        {
            return completionFired.load();
        }
    ));
    EXPECT_TRUE(observedAllWrites);
}
