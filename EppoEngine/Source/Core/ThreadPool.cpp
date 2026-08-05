#include "pch.h"
#include "Core/ThreadPool.h"

#include <algorithm>

namespace Eppo
{
    ThreadPool::ThreadPool()
        : m_OwnerThread(std::this_thread::get_id())
    {
        const uint32_t threadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
        m_Threads.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; i++)
        {
            m_Threads.emplace_back(
                [this]() -> void
                {
                    WorkerLoop();
                }
            );
        }
    }

    ThreadPool::~ThreadPool()
    {
        Shutdown(true);
    }

    auto ThreadPool::QueueTask(std::string name, TaskFn taskFn, CompletionFn completionFn, TaskPriority priority) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTask")

        const TaskId id = m_NextTaskId.fetch_add(1);

        auto task = CreateRef<Task>();
        task->Id = id;
        task->Name = std::move(name);
        task->Priority = priority;
        task->Fn = std::move(taskFn);
        task->OnComplete = std::move(completionFn);

        {
            std::scoped_lock lock(m_PendingMutex);
            m_PendingTasks.at(static_cast<size_t>(priority)).emplace_back(task);
            m_AllTasks[id] = task;
            m_TasksPending++;
        }

        m_WorkAvailableCV.notify_one();
        return id;
    }

    auto ThreadPool::QueueTaskWithDependencies(
        std::string name, TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority
    ) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTaskWithDependencies")

        const TaskId id = m_NextTaskId.fetch_add(1);

        auto task = CreateRef<Task>();
        task->Id = id;
        task->Name = std::move(name);
        task->Fn = std::move(taskFn);
        task->OnComplete = std::move(completionFn);
        task->Priority = priority;

        // NOTE: Currently if dependencies have a low priority, it might take a long while for a high priority dependent to run
        {
            std::scoped_lock lock(m_PendingMutex);

            for (const auto& dependencyId : dependencies)
            {
                if (dependencyId == 0 || dependencyId >= id)
                {
                    Log::Error("Task '{}' depends on task id {} which was never issued!", task->Name, dependencyId);
                    return 0;
                }
            }

            uint32_t remainingDeps = 0;
            for (const auto& dependencyId : dependencies)
            {
                if (m_AllTasks.contains(dependencyId))
                    continue;

                const auto status = m_AllTasks.at(dependencyId)->Status.load(std::memory_order_relaxed);
                if (status == TaskStatus::Completed || status == TaskStatus::Failed || status == TaskStatus::Cancelled)
                    continue;

                m_AllTasks.at(dependencyId)->Dependents.emplace_back(id);
                remainingDeps++;
            }

            task->RemainingDeps = remainingDeps;
            if (remainingDeps == 0)
                m_PendingTasks.at(static_cast<size_t>(priority)).emplace_back(task);
            m_AllTasks[id] = task;
            m_TasksPending++;
        }

        m_WorkAvailableCV.notify_one();
        return id;
    }

    auto ThreadPool::Flush() -> uint32_t
    {
        EP_PROFILE_FN("ThreadPool::Flush")
        EP_ASSERT(std::this_thread::get_id() == m_OwnerThread, "ThreadPool::Flush is main thread only!");

        std::vector<Ref<Task>> batch;
        {
            std::scoped_lock lock(m_CompletedMutex);
            const auto count = m_CompletedTasks.size();
            batch.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                batch.emplace_back(std::move(m_CompletedTasks.front()));
                m_CompletedTasks.pop_front();
            }
        }

        std::vector<TaskId> completedTaskIds(batch.size());
        for (size_t i = 0; i < batch.size(); i++)
        {
            auto& task = batch.at(i);

            if (task->OnComplete)
                task->OnComplete(task->Status);

            completedTaskIds[i] = task->Id;
        }

        std::scoped_lock lock(m_PendingMutex);
        for (size_t i = 0; i < completedTaskIds.size(); i++)
            m_AllTasks.erase(completedTaskIds.at(i));

        return static_cast<uint32_t>(batch.size());
    }

    auto ThreadPool::CancelAll() -> void
    {
        EP_PROFILE_FN("ThreadPool::CancelAll")

        std::scoped_lock lock(m_PendingMutex);

        for (auto& [taskId, task] : m_AllTasks)
        {
            if (task->Status == TaskStatus::Pending)
                task->Status = TaskStatus::Cancelled;
        }
    }

    auto ThreadPool::Shutdown(bool cancelPending) -> void
    {
        EP_PROFILE_FN("ThreadPool::Shutdown")

        if (cancelPending)
            CancelAll();

        {
            std::scoped_lock lock(m_PendingMutex);
            m_IsRunning = false;
        }
        m_WorkAvailableCV.notify_all();

        for (auto& thread : m_Threads)
            thread.join();
        m_Threads.clear();

        // We are now single threaded so we can safely access tasks without a mutex
        Flush();
    }

    auto ThreadPool::GetPendingTasksCount() const -> uint32_t
    {
        return m_TasksPending.load(std::memory_order_relaxed) + m_TasksInFlight.load(std::memory_order_relaxed);
    }

    auto ThreadPool::WorkerLoop() -> void
    {
        while (true)
        {
            Ref<Task> task;

            // Wait for task
            {
                // TODO: Why unique?
                std::unique_lock lock(m_PendingMutex);
                m_WorkAvailableCV.wait(
                    lock,
                    [this]() -> bool
                    {
                        return !m_IsRunning || HasPendingTasks();
                    }
                );

                if (!m_IsRunning && !HasPendingTasks())
                    return;

                task = GetNextTask();
            }

            if (task->Status.load(std::memory_order_relaxed) == TaskStatus::Running)
            {
                try
                {
                    task->Fn();
                    task->Status.store(TaskStatus::Completed, std::memory_order_relaxed);
                }
                catch (const std::exception& e)
                {
                    Log::Error("Task '{}' with id {} threw: {}", task->Name, task->Id, e.what());
                    task->Status.store(TaskStatus::Failed, std::memory_order_relaxed);
                }
                catch (...)
                {
                    Log::Error("Task '{}' with id {} threw unknown exception!", task->Name, task->Id);
                    task->Status.store(TaskStatus::Failed, std::memory_order_relaxed);
                }
            }

            // Process task dependencies
            if (!task->Dependents.empty())
            {
                std::scoped_lock lock(m_PendingMutex);
                for (const auto& dependentId : task->Dependents)
                {
                    if (m_AllTasks.contains(dependentId))
                        continue;

                    auto& dependentTask = m_AllTasks.at(dependentId);

                    // fetch_sub returns the value from *before* the subtraction, so the last dependency
                    // to resolve sees 1, not 0.
                    const uint32_t remaining = dependentTask->RemainingDeps.fetch_sub(1, std::memory_order_relaxed);
                    if (remaining == 1)
                    {
                        m_PendingTasks.at(static_cast<size_t>(dependentTask->Priority)).emplace_back(dependentTask);
                        m_WorkAvailableCV.notify_one();
                    }
                }
            }

            // Add to completed tasks
            {
                std::scoped_lock lock(m_CompletedMutex);
                m_CompletedTasks.emplace_back(task);
                m_TasksInFlight.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    auto ThreadPool::HasPendingTasks() const -> bool
    {
        // Called inside a lock, no lock needed
        for (const auto& queue : m_PendingTasks)
        {
            if (!queue.empty())
                return true;
        }

        return false;
    }

    auto ThreadPool::GetNextTask() -> Ref<Task>
    {
        EP_PROFILE_FN("ThreadPool::GetNextTask")

        // Called inside a lock, no lock needed
        // Run in reverse so highest priority gets selected first
        for (auto it = m_PendingTasks.rbegin(); it != m_PendingTasks.rend(); ++it)
        {
            if (it->empty())
                continue;

            auto task = std::move(it->front());
            it->pop_front();

            // Claim it while the queue lock is still held, so CancelAll can no longer reach it. A task it
            // already cancelled keeps that status and its body is skipped.
            auto expected = TaskStatus::Pending;
            task->Status.compare_exchange_strong(expected, TaskStatus::Running, std::memory_order_relaxed);

            m_TasksInFlight.fetch_add(1, std::memory_order_relaxed);
            m_TasksPending.fetch_sub(1, std::memory_order_relaxed);
            return task;
        }

        return nullptr;
    }
}
