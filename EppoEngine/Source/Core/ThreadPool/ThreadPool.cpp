#include "pch.h"
#include "Core/ThreadPool/ThreadPool.h"

#include <algorithm>

namespace Eppo
{
    TaskGroupSnapshot::TaskGroupSnapshot(const TaskGroupSnapshot& other)
    {
        *this = other;
    }

    auto TaskGroupSnapshot::operator=(const TaskGroupSnapshot& other) -> TaskGroupSnapshot&
    {
        if (this == &other)
            return *this;

        while (true)
        {
            const auto version = other.m_Version.load(std::memory_order_seq_cst);
            if ((version & 1u) != 0)
            {
                std::this_thread::yield();
                continue;
            }

            const auto name = other.Name;
            const auto pending = other.Pending.load(std::memory_order_seq_cst);
            const auto running = other.Running.load(std::memory_order_seq_cst);
            const auto completed = other.Completed.load(std::memory_order_seq_cst);
            const auto failed = other.Failed.load(std::memory_order_seq_cst);
            const auto cancelled = other.Cancelled.load(std::memory_order_seq_cst);
            const auto total = other.Total.load(std::memory_order_seq_cst);

            if (version != other.m_Version.load(std::memory_order_seq_cst))
                continue;

            Name = name;
            Pending.store(pending, std::memory_order_relaxed);
            Running.store(running, std::memory_order_relaxed);
            Completed.store(completed, std::memory_order_relaxed);
            Failed.store(failed, std::memory_order_relaxed);
            Cancelled.store(cancelled, std::memory_order_relaxed);
            Total.store(total, std::memory_order_relaxed);
            m_Version.store(0, std::memory_order_relaxed);
            return *this;
        }
    }

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

    auto ThreadPool::QueueTask(TaskFn taskFn, CompletionFn completionFn, TaskPriority priority) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTask")
        return QueueTaskInternal(std::move(taskFn), std::move(completionFn), {}, priority);
    }

    auto ThreadPool::QueueTask(std::string name, TaskFn taskFn, CompletionFn completionFn, TaskPriority priority) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTask")
        return QueueTaskInternal(std::move(name), std::move(taskFn), std::move(completionFn), {}, priority);
    }

    auto ThreadPool::QueueTaskWithDependencies(
        TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority
    ) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTaskWithDependencies")
        return QueueTaskInternal(std::move(taskFn), std::move(completionFn), dependencies, priority);
    }

    auto ThreadPool::QueueTaskWithDependencies(
        std::string name, TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority
    ) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTaskWithDependencies")
        return QueueTaskInternal(std::move(name), std::move(taskFn), std::move(completionFn), dependencies, priority);
    }

    auto
    ThreadPool::QueueTaskInternal(TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority)
        -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTaskInternal")

        const TaskId id = m_NextTaskId.fetch_add(1, std::memory_order_relaxed);

        auto task = CreateRef<Task>();
        task->Id = id;
        task->Fn = std::move(taskFn);
        task->OnComplete = std::move(completionFn);
        task->Priority = priority;

        // NOTE: Currently if dependencies have a low priority, it might take a long while for a high priority dependent to run
        bool isReady = false;
        {
            std::scoped_lock lock(m_PendingMutex);
            if (!m_IsRunning.load(std::memory_order_relaxed))
            {
                Log::Warn("Tried to queue task after thread pool shutdown!");
                return 0;
            }

            for (const auto dependencyId : dependencies)
            {
                if (dependencyId >= id)
                {
                    Log::Error("Task with id {} depends on task id {} which was never issued!", id, dependencyId);
                    return 0;
                }
            }

            uint32_t remainingDeps = 0;
            for (const auto dependencyId : dependencies)
            {
                const auto dependencyIt = m_AllTasks.find(dependencyId);
                if (dependencyIt == m_AllTasks.end())
                    continue;

                const auto status = dependencyIt->second->Status.load(std::memory_order_relaxed);
                if (status == TaskStatus::Completed || status == TaskStatus::Failed || status == TaskStatus::Cancelled)
                    continue;

                dependencyIt->second->Dependents.emplace_back(task);
                remainingDeps++;
            }

            task->RemainingDeps.store(remainingDeps, std::memory_order_relaxed);
            isReady = remainingDeps == 0;
            if (isReady)
                m_PendingTasks.at(static_cast<size_t>(priority)).emplace_back(task);
            m_AllTasks[id] = task;
            m_TasksPending.fetch_add(1, std::memory_order_seq_cst);
        }

        if (isReady)
            m_WorkAvailableCV.notify_one();
        return id;
    }

    auto ThreadPool::QueueTaskInternal(
        std::string name, TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority
    ) -> TaskId
    {
        EP_PROFILE_FN("ThreadPool::QueueTaskInternal")

        const TaskId id = m_NextTaskId.fetch_add(1, std::memory_order_relaxed);

        auto task = CreateRef<Task>();
        task->Id = id;
        task->Name = std::move(name);
        task->Fn = std::move(taskFn);
        task->OnComplete = std::move(completionFn);
        task->Priority = priority;

        // NOTE: Currently if dependencies have a low priority, it might take a long while for a high priority dependent to run
        bool isReady = false;
        {
            std::scoped_lock lock(m_PendingMutex);
            if (!m_IsRunning.load(std::memory_order_relaxed))
            {
                Log::Warn("Tried to queue task '{}' after thread pool shutdown!", task->Name);
                return 0;
            }

            for (const auto dependencyId : dependencies)
            {
                if (dependencyId >= id)
                {
                    Log::Error("Task '{}' depends on task id {} which was never issued!", task->Name, dependencyId);
                    return 0;
                }
            }

            uint32_t remainingDeps = 0;
            for (const auto dependencyId : dependencies)
            {
                const auto dependencyIt = m_AllTasks.find(dependencyId);
                if (dependencyIt == m_AllTasks.end())
                    continue;

                const auto status = dependencyIt->second->Status.load(std::memory_order_relaxed);
                if (status == TaskStatus::Completed || status == TaskStatus::Failed || status == TaskStatus::Cancelled)
                    continue;

                dependencyIt->second->Dependents.emplace_back(task);
                remainingDeps++;
            }

            {
                std::scoped_lock snapshotLock(m_SnapshotMutex);
                const auto [snapshotIt, inserted] = m_Snapshots.try_emplace(task->Name);
                if (inserted)
                {
                    snapshotIt->second = CreateRef<TaskGroupSnapshot>();
                    snapshotIt->second->Name = task->Name;
                }
                task->Group = snapshotIt->second;
            }
            UpdateTaskGroup(task->Group, TaskStatus::Pending);

            task->RemainingDeps.store(remainingDeps, std::memory_order_relaxed);
            isReady = remainingDeps == 0;
            if (isReady)
                m_PendingTasks.at(static_cast<size_t>(priority)).emplace_back(task);
            m_AllTasks[id] = task;
            m_TasksPending.fetch_add(1, std::memory_order_seq_cst);
        }

        if (isReady)
            m_WorkAvailableCV.notify_one();
        return id;
    }


    auto ThreadPool::GetTaskGroupSnapshots() -> std::unordered_map<std::string, TaskGroupSnapshot>
    {
        EP_PROFILE_FN("ThreadPool::GetTaskGroupSnapshots")

        std::shared_lock lock(m_SnapshotMutex);

        std::unordered_map<std::string, TaskGroupSnapshot> snapshots;
        snapshots.reserve(m_Snapshots.size());
        for (const auto& [name, snapshot] : m_Snapshots)
            snapshots.emplace(name, *snapshot);

        return snapshots;
    }

    auto ThreadPool::UpdateTaskGroup(const Ref<TaskGroupSnapshot>& group, const TaskStatus status) -> void
    {
        if (!group)
            return;

        auto version = group->m_Version.load(std::memory_order_seq_cst);
        while (true)
        {
            if ((version & 1u) != 0)
            {
                std::this_thread::yield();
                version = group->m_Version.load(std::memory_order_seq_cst);
                continue;
            }

            if (group->m_Version.compare_exchange_weak(version, version + 1, std::memory_order_seq_cst, std::memory_order_seq_cst))
                break;
        }

        switch (status)
        {
            case TaskStatus::Pending:
                group->Pending.fetch_add(1, std::memory_order_seq_cst);
                group->Total.fetch_add(1, std::memory_order_seq_cst);
                break;
            case TaskStatus::Running:
                EP_ASSERT(group->Pending.load(std::memory_order_relaxed) > 0, "Task group has no pending task to start!");
                group->Pending.fetch_sub(1, std::memory_order_seq_cst);
                group->Running.fetch_add(1, std::memory_order_seq_cst);
                break;
            case TaskStatus::Completed:
                EP_ASSERT(group->Running.load(std::memory_order_relaxed) > 0, "Task group has no running task to complete!");
                group->Running.fetch_sub(1, std::memory_order_seq_cst);
                group->Completed.fetch_add(1, std::memory_order_seq_cst);
                break;
            case TaskStatus::Failed:
                EP_ASSERT(group->Running.load(std::memory_order_relaxed) > 0, "Task group has no running task to fail!");
                group->Running.fetch_sub(1, std::memory_order_seq_cst);
                group->Failed.fetch_add(1, std::memory_order_seq_cst);
                break;
            case TaskStatus::Cancelled:
                EP_ASSERT(group->Pending.load(std::memory_order_relaxed) > 0, "Task group has no pending task to cancel!");
                group->Pending.fetch_sub(1, std::memory_order_seq_cst);
                group->Cancelled.fetch_add(1, std::memory_order_seq_cst);
                break;
        }

        group->m_Version.store(version + 2, std::memory_order_seq_cst);
    }

    auto ThreadPool::FinalizeTask(const Ref<Task>& task, const TaskStatus status) -> void
    {
        std::vector<Ref<Task>> dependents;
        {
            std::scoped_lock lock(m_PendingMutex);
            if (status != TaskStatus::Cancelled)
                task->Status.store(status, std::memory_order_relaxed);
            dependents = std::move(task->Dependents);
        }

        std::vector<Ref<Task>> readyTasks;
        readyTasks.reserve(dependents.size());
        for (const auto& dependent : dependents)
        {
            const auto remaining = dependent->RemainingDeps.fetch_sub(1, std::memory_order_acq_rel);
            EP_ASSERT(remaining > 0, "Task dependency counter underflowed!");
            if (remaining == 1)
                readyTasks.emplace_back(dependent);
        }

        if (!readyTasks.empty())
        {
            {
                std::scoped_lock lock(m_PendingMutex);
                for (const auto& readyTask : readyTasks)
                {
                    if (readyTask->Status.load(std::memory_order_relaxed) != TaskStatus::Pending)
                        continue;
                    m_PendingTasks.at(static_cast<size_t>(readyTask->Priority)).emplace_back(readyTask);
                }
            }

            m_WorkAvailableCV.notify_all();
        }

        CompleteTask(task, status);
    }

    auto ThreadPool::CompleteTask(const Ref<Task>& task, const TaskStatus status) -> void
    {
        UpdateTaskGroup(task->Group, status);

        {
            std::scoped_lock lock(m_CompletedMutex);
            m_CompletedTasks.emplace_back(task);
        }

        if (status == TaskStatus::Cancelled)
            m_TasksPending.fetch_sub(1, std::memory_order_seq_cst);
        else
            m_TasksInFlight.fetch_sub(1, std::memory_order_seq_cst);
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
            {
                try
                {
                    task->OnComplete(task->Status.load(std::memory_order_relaxed));
                }
                catch (const std::exception& e)
                {
                    Log::Error("Completion callback for task '{}' with id {} threw: {}", task->Name, task->Id, e.what());
                }
                catch (...)
                {
                    Log::Error("Completion callback for task '{}' with id {} threw unknown exception!", task->Name, task->Id);
                }
            }

            completedTaskIds[i] = task->Id;
        }

        {
            std::scoped_lock lock(m_PendingMutex);
            for (const auto taskId : completedTaskIds)
                m_AllTasks.erase(taskId);
        }

        return static_cast<uint32_t>(batch.size());
    }

    auto ThreadPool::CancelTask(TaskId taskId) -> bool
    {
        EP_PROFILE_FN("ThreadPool::CancelTask")

        Ref<Task> task = nullptr;

        {
            std::scoped_lock lock(m_PendingMutex);

            const auto taskIt = m_AllTasks.find(taskId);
            if (taskIt == m_AllTasks.end())
                return false;
            task = taskIt->second;
        }

        auto expected = TaskStatus::Pending;
        if (!task->Status.compare_exchange_strong(expected, TaskStatus::Cancelled, std::memory_order_relaxed))
            return false;

        FinalizeTask(task, TaskStatus::Cancelled);
        return true;
    }

    auto ThreadPool::CancelAll() -> void
    {
        EP_PROFILE_FN("ThreadPool::CancelAll")

        std::vector<Ref<Task>> cancelledTasks;
        {
            std::scoped_lock lock(m_PendingMutex);

            cancelledTasks.reserve(m_AllTasks.size());
            for (const auto& [taskId, task] : m_AllTasks)
            {
                auto expected = TaskStatus::Pending;
                if (task->Status.compare_exchange_strong(expected, TaskStatus::Cancelled, std::memory_order_relaxed))
                    cancelledTasks.emplace_back(task);
            }
        }

        for (const auto& task : cancelledTasks)
            FinalizeTask(task, TaskStatus::Cancelled);
    }

    auto ThreadPool::Shutdown(bool cancelPending) -> void
    {
        EP_PROFILE_FN("ThreadPool::Shutdown")

        std::vector<Ref<Task>> cancelledTasks;
        {
            std::scoped_lock lock(m_PendingMutex);
            m_IsRunning.store(false, std::memory_order_relaxed);

            if (cancelPending)
            {
                cancelledTasks.reserve(m_AllTasks.size());
                for (const auto& [taskId, task] : m_AllTasks)
                {
                    auto expected = TaskStatus::Pending;
                    if (task->Status.compare_exchange_strong(expected, TaskStatus::Cancelled, std::memory_order_relaxed))
                        cancelledTasks.emplace_back(task);
                }
            }
        }

        for (const auto& task : cancelledTasks)
            FinalizeTask(task, TaskStatus::Cancelled);

        m_WorkAvailableCV.notify_all();

        for (auto& thread : m_Threads)
            thread.join();
        m_Threads.clear();

        // We are now single threaded so we can safely access tasks without a mutex
        Flush();
    }

    auto ThreadPool::GetPendingTasksCount() const -> uint32_t
    {
        return m_TasksPending.load(std::memory_order_seq_cst) + m_TasksInFlight.load(std::memory_order_seq_cst);
    }

    auto ThreadPool::WorkerLoop() -> void
    {
        while (true)
        {
            Ref<Task> task;

            // Wait for task
            {
                std::unique_lock lock(m_PendingMutex);
                m_WorkAvailableCV.wait(
                    lock,
                    [this]() -> bool
                    {
                        return !m_IsRunning.load(std::memory_order_relaxed) || HasPendingTasks();
                    }
                );

                if (!m_IsRunning.load(std::memory_order_relaxed) && !HasPendingTasks())
                    return;

                task = GetNextTask();
            }

            if (!task)
                continue;

            UpdateTaskGroup(task->Group, TaskStatus::Running);

            auto taskStatus = TaskStatus::Completed;
            try
            {
                task->Fn();
            }
            catch (const std::exception& e)
            {
                Log::Error("Task '{}' with id {} threw: {}", task->Name, task->Id, e.what());
                taskStatus = TaskStatus::Failed;
            }
            catch (...)
            {
                Log::Error("Task '{}' with id {} threw unknown exception!", task->Name, task->Id);
                taskStatus = TaskStatus::Failed;
            }

            FinalizeTask(task, taskStatus);
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
            while (!it->empty())
            {
                auto task = std::move(it->front());
                it->pop_front();

                auto expected = TaskStatus::Pending;
                if (!task->Status.compare_exchange_strong(expected, TaskStatus::Running, std::memory_order_relaxed))
                    continue;

                m_TasksInFlight.fetch_add(1, std::memory_order_seq_cst);
                m_TasksPending.fetch_sub(1, std::memory_order_seq_cst);
                return task;
            }
        }

        return nullptr;
    }
}
