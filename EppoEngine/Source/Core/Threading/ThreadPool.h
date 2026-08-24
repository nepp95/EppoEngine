#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
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
        TaskGroupSnapshot() = default;
        TaskGroupSnapshot(const TaskGroupSnapshot& other);
        auto operator=(const TaskGroupSnapshot& other) -> TaskGroupSnapshot&;

        std::string Name;
        std::atomic<uint32_t> Pending = 0;
        std::atomic<uint32_t> Running = 0;
        std::atomic<uint32_t> Completed = 0;
        std::atomic<uint32_t> Failed = 0;
        std::atomic<uint32_t> Cancelled = 0;
        std::atomic<uint32_t> Total = 0;

        auto IsFinished() const -> bool
        {
            const auto total = Total.load(std::memory_order_relaxed);
            const auto finished = Completed.load(std::memory_order_relaxed) + Failed.load(std::memory_order_relaxed) +
                Cancelled.load(std::memory_order_relaxed);
            return finished >= total;
        }

    private:
        std::atomic<uint64_t> m_Version = 0;

        friend class ThreadPool;
    };

    template<typename T>
    using TaskResult = std::optional<T>;

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

        // Callable: All threads
        auto CancelTask(TaskId taskId) -> bool;

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
            std::vector<Ref<Task>> Dependents;
            std::atomic<uint32_t> RemainingDeps = 0;

            Ref<TaskGroupSnapshot> Group;
        };

        auto QueueTaskInternal(TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority)
            -> TaskId;
        auto QueueTaskInternal(
            std::string name, TaskFn taskFn, CompletionFn completionFn, const std::vector<TaskId>& dependencies, TaskPriority priority
        ) -> TaskId;
        auto AddTaskToGroup(const Ref<Task>& task, const Ref<TaskGroupSnapshot>& group) -> void;
        auto UpdateTaskGroup(const Ref<TaskGroupSnapshot>& group, TaskStatus status) -> void;
        auto FinalizeTask(const Ref<Task>& task, TaskStatus status) -> void;
        auto CompleteTask(const Ref<Task>& task, TaskStatus status) -> void;
        auto WorkerLoop(uint32_t index) -> void;
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
        std::unordered_map<std::string, Ref<TaskGroupSnapshot>> m_Snapshots;

        // Workpool
        std::vector<std::thread> m_Threads;
        std::condition_variable m_WorkAvailableCV;
        std::atomic<bool> m_IsRunning = true;
        std::atomic<uint32_t> m_TasksInFlight = 0;
        std::atomic<TaskId> m_NextTaskId = 1;
        std::thread::id m_OwnerThread;
    };
}
