#pragma once

#include "TaskGraph_base.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <any>
#include <chrono>
#include <typeinfo>
#include <stdexcept>

namespace Log { class LogObject; }

namespace TaskGraph
{
    class TaskScheduler;
    class Task;
    class TaskGroup;

    /// <summary>
    /// Execution context handed to a task body. Provides result set/get, cancel
    /// polling, dependency-result access and dynamic child spawning. Only the
    /// currently running task should use its own context; do not store or share.
    /// </summary>
    class TASK_GRAPH_API TaskContext
    {
        public:
        TaskContext(Task* task, TaskScheduler* scheduler)
            : m_task(task), m_scheduler(scheduler) {}

        /// <summary>
        /// Virtual so an app-supplied derived context can be owned and destroyed
        /// polymorphically through a base TaskContext pointer (see
        /// TaskScheduler::setContextFactory).
        /// </summary>
        virtual ~TaskContext();

        void setResult(std::any value);
        bool isCancelRequested() const;

        /// <summary>
        /// Read a completed dependency's result. Throws std::bad_any_cast on type mismatch,
        /// throws std::runtime_error if the dependency has no result set.
        /// </summary>
        template <class T>
        T getDependencyResult(const Task& dep) const;

        /// <summary>
        /// Insert a new task into the running scheduler with the caller task as an
        /// implicit predecessor (child runs after the caller finishes). Any explicit
        /// dependencies on the child must be configured before this call. Returns
        /// false if the scheduler is not running or registration failed.
        /// </summary>
        bool spawn(const std::shared_ptr<Task>& child);

        /// <summary>Access the running task's per-instance logger.</summary>
        Log::LogObject& log();

        /// <summary>
        /// Block the calling worker until the GUI thread responds via
        /// TaskScheduler::respondToGuiEvent. On cancellation the wait returns an
        /// invalid QVariant. MUST NOT be called from the GUI thread or from a
        /// Task with TaskAffinity::Gui — either would deadlock. This primitive
        /// is the first scheduler-provided blocking point that is
        /// cancellation-aware; other blocking work in a task body must poll
        /// isCancelRequested() cooperatively.
        /// </summary>
        QVariant askGui(const QVariant& payload);

        template <class T>
        T askGuiAs(const QVariant& payload)
        {
            return askGui(payload).value<T>();
        }

        Task* task() const { return m_task; }

        private:
        Task* m_task;
        TaskScheduler* m_scheduler;
    };

    /// <summary>
    /// A task is a workblock which can be run by the TaskScheduler.
    /// Each task should be processable by a single thread.
    /// A task can have dependencies on other tasks, which must be processed before this task can be run.
    /// Do not interact between tasks, because they can be processed in parallel.
    /// </summary>
    class TASK_GRAPH_API Task : public QObject
    {
        Q_OBJECT
        public:

        enum class Status : int
        {
            Pending = 0,
            Ready,
            Running,
            Done,
            Failed,
            Cancelled,
            Skipped
        };

        enum class TaskAffinity : int
        {
            Any = 0,
            Gui
        };

        Task();
        Task(const std::string &name);
        Task(const Task& other) = delete;
        Task& operator=(const Task& other) = delete;
        virtual ~Task();

        const std::string& getName() const { return m_name; }
        void setName(const std::string& name);

        Log::LogObject& logger();
        const Log::LogObject& logger() const;

        /// <summary>
        /// Route this task's logging through an external, caller-supplied logger instead of the
        /// task-owned one. The Task does NOT take ownership. Pass nullptr to revert to the
        /// task-owned logger. When set before first logger use, the task never constructs its own
        /// LogObject (so no empty per-task context is registered).
        /// </summary>
        void setExternalLogger(Log::LogObject* logger);
        bool hasExternalLogger() const;

        void setWorkFunction(const std::function<void()>& workFunction) { m_workFunction = workFunction; }
        void setWorkFunction(const std::function<void(TaskContext&)>& workFunction) { m_workFunctionCtx = workFunction; }

        void setAffinity(TaskAffinity a) { m_affinity.store(a, std::memory_order_release); }
        TaskAffinity getAffinity() const { return m_affinity.load(std::memory_order_acquire); }

        /// <summary>Per-task progress weight; must be positive. Default 1.0.</summary>
        void setWeight(float w);
        float getWeight() const { return m_weight.load(std::memory_order_acquire); }

        /// <summary>Wall-clock timeout for the task body. Zero (default) disables the watchdog.</summary>
        void setTimeout(std::chrono::milliseconds t) { m_timeoutMs.store(t.count(), std::memory_order_release); }
        std::chrono::milliseconds getTimeout() const { return std::chrono::milliseconds(m_timeoutMs.load(std::memory_order_acquire)); }

        void setMaxRetries(int n) { m_maxRetries.store(n < 0 ? 0 : n, std::memory_order_release); }
        int getMaxRetries() const { return m_maxRetries.load(std::memory_order_acquire); }

        void setRetryBackoff(std::chrono::milliseconds t) { m_backoffMs.store(t.count(), std::memory_order_release); }
        std::chrono::milliseconds getRetryBackoff() const { return std::chrono::milliseconds(m_backoffMs.load(std::memory_order_acquire)); }

        bool addDependency(const std::shared_ptr<Task>& task);
        /// <summary>Depend on every member of the group. Returns true only if all members were added successfully.</summary>
        bool addDependency(const TaskGroup& group);
        bool clearDependencies();
        std::vector<std::shared_ptr<Task>> getDependencies() const;

        bool runTask();
        bool runTask(TaskContext* ctx);

        Status getStatus() const { return m_status.load(std::memory_order_acquire); }
        bool isRunning() const { return getStatus() == Status::Running; }
        bool isDone() const { return getStatus() == Status::Done; }
        bool isTerminal() const
        {
            Status s = getStatus();
            return s == Status::Done || s == Status::Failed
                || s == Status::Cancelled || s == Status::Skipped;
        }

        void cancel();
        bool isCancelRequested() const { return m_cancelRequested.load(std::memory_order_acquire); }

        void skip();
        void reset();

        bool checkDependencies();
        QString getLastError() const;

        /// <summary>
        /// Read-only access to the task's result (empty std::any if never set).
        /// Safe to call once the task has reached a terminal state.
        /// </summary>
        std::any getResult() const;

        /// <summary>
        /// Store a result. Only valid to call while the task is Running (from within its body).
        /// </summary>
        void setResult(std::any value);

        // Internal — used by TaskScheduler for retry/timeout bookkeeping. Not part of the public contract.
        void signalTimeout();
        void prepareRetry();
        uint64_t beginTimeoutWindow() { return m_timeoutGen.fetch_add(1, std::memory_order_acq_rel) + 1; }
        uint64_t currentTimeoutWindow() const { return m_timeoutGen.load(std::memory_order_acquire); }

        signals:
        void started();
        void completed();
        void failed(QString error);
        void wasReset();

        protected:
        virtual void work();
        virtual void work(TaskContext& ctx);

        private:
        bool wouldCreateCycle(const std::shared_ptr<Task>& candidate) const;
        void setLastError(const QString& err);

        // Own: lazy task-owned logger (default). External: caller-supplied logger.
        // None: no logging — no own LogObject, internal run logs suppressed.
        enum class LoggerMode { Own, External, None };

        Log::LogObject& effectiveLogger() const;
        Log::LogObject* effectiveLoggerOrNull() const;

        std::string m_name;
        std::atomic<Status> m_status;
        std::atomic<TaskAffinity> m_affinity;
        std::atomic<bool> m_cancelRequested;
        std::atomic<bool> m_timeoutHit;
        std::atomic<uint64_t> m_timeoutGen;

        std::atomic<float> m_weight;
        std::atomic<int64_t> m_timeoutMs;
        std::atomic<int> m_maxRetries;
        std::atomic<int64_t> m_backoffMs;

        mutable std::unique_ptr<Log::LogObject> m_logger;
        Log::LogObject* m_externalLogger = nullptr;
        LoggerMode m_loggerMode = LoggerMode::Own;
        mutable std::mutex m_loggerMutex;

        std::function<void()> m_workFunction;
        std::function<void(TaskContext&)> m_workFunctionCtx;
        std::vector<std::weak_ptr<Task>> m_dependencies;
        mutable std::mutex m_depMutex;

        mutable std::mutex m_errorMutex;
        QString m_lastError;
        std::any m_result;
    };

    /// <summary>
    /// Read a completed task's result as T. Throws std::bad_any_cast on mismatch.
    /// </summary>
    template <class T>
    T getResultAs(const Task& task)
    {
        std::any r = task.getResult();
        return std::any_cast<T>(r);
    }

    template <class T>
    T TaskContext::getDependencyResult(const Task& dep) const
    {
        std::any r = dep.getResult();
        if (!r.has_value())
            throw std::runtime_error("Dependency has no result");
        return std::any_cast<T>(r);
    }
}
