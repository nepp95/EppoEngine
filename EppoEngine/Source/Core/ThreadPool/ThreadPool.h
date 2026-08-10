#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace Eppo
{
    using TaskId = uint64_t;

    enum class TaskPriority : uint8_t
    {
        Low = 0,
        Medium,
        High,
    };

    enum class TaskStatus : uint8_t
    {
        Pending = 0,
        Running,
        Completed,
        Failed,
        Cancelled,
    };

    struct TaskGroupSnapshot
    {
        std::string Name;
        uint32_t Pending = 0;
        uint32_t Running = 0;
        uint32_t Completed = 0;
        uint32_t Failed = 0;
        uint32_t Cancelled = 0;
        uint32_t Total = 0;

        auto IsFinished() const -> bool
        {
            auto remaining = Total - Completed - Failed - Cancelled;
            return remaining == 0;
        }
    };

    using TaskFn = std::function<void()>;
    using CompletionFn = std::function<void(TaskStatus)>;

    class ThreadPool
    {
    public:
        ThreadPool();
        ~ThreadPool();

        // Callable: All threads
        auto QueueTask(TaskFn taskFn, CompletionFn completionFn, TaskPriority priority = TaskPriority::Medium) -> TaskId;
        auto QueueTask(std::string name, TaskFn taskFn, CompletionFn completionFn, TaskPriority priority = TaskPriority::Medium) -> TaskId;
        auto QueueTaskWithDependencies(
            TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority = TaskPriority::Medium
        ) -> TaskId;
        auto QueueTaskWithDependencies(
            std::string name, TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies,
            TaskPriority priority = TaskPriority::Medium
        ) -> TaskId;

        // Callable: All threads
        auto GetTaskGroupSnapshots() -> std::unordered_map<std::string, TaskGroupSnapshot>;

        // Callable: Main thread
        auto Flush() -> uint32_t;

        // Callable: Main thread
        auto CancelAll() -> void;

        // Callable: Main thread
        auto Shutdown(bool cancelPending) -> void;

        // Callable: All threads
        [[nodiscard]] auto GetPendingTasksCount() const -> uint32_t;

    private:
        struct Task
        {
            TaskId Id = 0;
            std::string Name;
            TaskFn Fn;
            CompletionFn OnComplete;
            TaskPriority Priority = TaskPriority::Medium;
            std::atomic<TaskStatus> Status = TaskStatus::Pending;

            // Dependencies
            std::vector<TaskId> Dependents;
            std::atomic<uint32_t> RemainingDeps = 0;
        };

        auto WorkerLoop() -> void;
        [[nodiscard]] auto HasPendingTasks() const -> bool;
        [[nodiscard]] auto GetNextTask() -> Ref<Task>;

    private:
        // Pending tasks
        std::mutex m_PendingMutex;
        std::atomic<uint32_t> m_TasksPending = 0;
        std::array<std::deque<Ref<Task>>, 3> m_PendingTasks{};
        std::unordered_map<TaskId, Ref<Task>> m_AllTasks;

        // Completed tasks
        std::mutex m_CompletedMutex;
        std::deque<Ref<Task>> m_CompletedTasks;

        // Snapshotting
        std::shared_mutex m_SnapshotMutex;
        std::unordered_map<std::string, TaskGroupSnapshot> m_Snapshots;

        // Workpool
        std::vector<std::thread> m_Threads;
        std::condition_variable m_WorkAvailableCV;
        std::atomic<bool> m_IsRunning = true;
        std::atomic<uint32_t> m_TasksInFlight = 0;
        std::atomic<TaskId> m_NextTaskId = 1;
        std::thread::id m_OwnerThread;
    };
}
