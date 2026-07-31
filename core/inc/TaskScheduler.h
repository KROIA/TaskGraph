#pragma once

#include "TaskGraph_base.h"
#include "Task.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

namespace Log { class LogObject; }

namespace TaskGraph
{
    using TaskList = std::vector<std::shared_ptr<Task>>;
    using TaskDeque = std::deque<std::shared_ptr<Task>>;

    /// <summary>
    /// Lightweight collection of tasks treated as a single dependency unit.
    /// Depending on a group is syntactic sugar for depending on every member.
    /// </summary>
    class TASK_GRAPH_API TaskGroup
    {
        public:
        TaskGroup() = default;
        explicit TaskGroup(const std::string& name) : m_name(name) {}

        void addTask(const std::shared_ptr<Task>& t)
        {
            if (t) m_members.push_back(t);
        }
        const std::vector<std::shared_ptr<Task>>& members() const { return m_members; }
        size_t size() const { return m_members.size(); }
        const std::string& getName() const { return m_name; }

        private:
        std::string m_name;
        std::vector<std::shared_ptr<Task>> m_members;
    };

    /// <summary>
    /// The TaskScheduler contains a list of tasks and runs them in dependency order.
    /// Independent tasks run in parallel via a ready-queue model: a task becomes
    /// eligible for execution as soon as its dependencies complete, without waiting
    /// for unrelated siblings.
    /// </summary>
    class TASK_GRAPH_API TaskScheduler : public QObject
    {
        Q_OBJECT
        public:
        enum Error
        {
            noError,
            noTasks,
            taskAlreadyAdded,
            missingDependency,
            dependencyGraphNotDAG,
            alreadyRunning,
            busy,
            __count
        };

        enum class FailurePolicy : int
        {
            FailFast = 0,
            ContinueOthers
        };

        TaskScheduler(size_t threadCount = std::thread::hardware_concurrency());
        ~TaskScheduler();

        bool enableThreads(size_t threadCount);
        bool disableThreads();

        bool addTask(const std::shared_ptr<Task>& task);
        bool removeTask(const std::shared_ptr<Task>& task);

        /// <summary>
        /// Factory that builds the TaskContext handed to a task body. Lets an app
        /// supply a derived context (extra per-run state) without templatizing the
        /// scheduler. The scheduler owns the returned context for the duration of a
        /// single task-run and passes it to the body as a base TaskContext&amp;.
        /// </summary>
        using ContextFactory = std::function<std::unique_ptr<TaskContext>(Task* task, TaskScheduler* scheduler)>;

        /// <summary>
        /// Install (or clear, by passing {}) the context factory. When set, it is
        /// invoked once per task-run to build that run's context; when unset, a base
        /// TaskContext is constructed. Thread-safety: the factory is invoked on the
        /// worker thread running the task, so it must be safe to call concurrently
        /// from multiple worker threads.
        /// </summary>
        void setContextFactory(ContextFactory factory) { m_contextFactory = std::move(factory); }

        void runTasks();
        void runTasksAsync();

        void resetTasks();
        void clear();

        void cancel();

        void pause();
        void resume();
        bool isPaused() const { return m_paused.load(std::memory_order_acquire); }

        void setFailurePolicy(FailurePolicy p) { m_failurePolicy.store(p, std::memory_order_release); }
        FailurePolicy getFailurePolicy() const { return m_failurePolicy.load(std::memory_order_acquire); }

        int getProgress() const { return m_progress; }
        float getProgressF() const { return m_progressF.load(std::memory_order_acquire); }
        bool isRunning() const { return m_isRunning.load(std::memory_order_acquire); }
        unsigned int getThreadCount() const { return static_cast<unsigned int>(m_threads.size()); }
        unsigned int getBusyThreadCount() const;
        size_t getTotalTasks() const;

        Error getLastError() const { return m_lastError.load(std::memory_order_acquire); }

        std::vector<TaskList> getTaskGraph() const;

        /// <summary>
        /// Dynamic edge insertion from within a running task body. `parent` must be the
        /// currently running caller task, already tracked. `child`'s explicit dependencies
        /// must already have been configured on `child` prior to this call. Returns false
        /// on any validation failure.
        /// </summary>
        bool addDynamicTask(const std::shared_ptr<Task>& child, Task* parent);

        Log::LogObject& logger();

        /// <summary>
        /// Blocking round-trip called from a worker task via TaskContext::askGui.
        /// Emits guiEventRequested and waits until respondToGuiEvent is called
        /// with the matching requestId or the task/graph is cancelled.
        /// Cancellation returns an invalid QVariant.
        /// </summary>
        QVariant waitForGuiResponse(int requestId, Task* task, const QVariant& payload);

        public slots:
        void respondToGuiEvent(int requestId, const QVariant& response);

        public:
        int allocateGuiRequestId();

        signals:
        void guiEventRequested(int requestId, QString taskName, QVariant payload);

        void started();
        void completed();
        void wasReset();
        void cancelled();
        void paused();
        void resumed();
        void progressUpdate(int progress);
        void progressChangedF(float progress);

        void statusMessage(QString msg);
        void taskStarted(QString taskName);
        void taskFinished(QString taskName);
        void taskFailed(QString taskName, QString error);
        void errorRaised(QString error);

        private:
        Error buildTaskGraph(std::vector<TaskList> &taskGraph) const;

        void ensureThreadsSpawned();
        void runTasksBody();
        void onTaskCompleted(const std::shared_ptr<Task>& task);
        void skipDescendantsLocked(Task* root);
        void cancelPendingLocked();

        // Arms a detached watchdog for `task` if a positive timeout is configured.
        void armWatchdog(const std::shared_ptr<Task>& task);

        TaskList m_allTasks;
        mutable std::vector<TaskList> m_taskGraph;

        std::vector<std::shared_ptr<std::thread>> m_threads;
        std::vector<std::shared_ptr<std::atomic<bool>>> m_threadExit;
        mutable std::mutex m_mutex;
        std::condition_variable m_cvTask;
        std::condition_variable m_cvComplete;
        bool m_stopThreads;
        unsigned int m_busyThreads;
        std::atomic<bool> m_isRunning;
        std::atomic<bool> m_paused;
        std::atomic<bool> m_cancelRequested;
        bool m_aborting;
        size_t m_desiredThreadCount;
        std::atomic<int> m_progress;
        std::atomic<float> m_progressF;

        TaskDeque m_readyQueue;
        std::unordered_map<Task*, int> m_inDegree;
        std::unordered_map<Task*, std::vector<Task*>> m_dependents;
        std::unordered_map<Task*, std::shared_ptr<Task>> m_aliveByPtr;
        size_t m_totalTasks;
        size_t m_remaining;

        double m_weightSum;
        double m_completedWeight;

        ContextFactory m_contextFactory;

        std::shared_ptr<std::thread> m_asyncThread = nullptr;
        mutable std::atomic<Error> m_lastError;
        std::atomic<FailurePolicy> m_failurePolicy;

        static void taskThreadFunction(TaskScheduler *obj, int threadIndex, std::shared_ptr<std::atomic<bool>> localExit);

        struct PendingGuiRequest
        {
            std::mutex m;
            std::condition_variable cv;
            QVariant response;
            bool ready = false;
            bool cancelled = false;
        };

        std::unique_ptr<Log::LogObject> m_logger;
        std::unordered_map<int, std::shared_ptr<PendingGuiRequest>> m_pendingGuiRequests;
        std::mutex m_pendingGuiMutex;
        std::atomic<int> m_nextGuiRequestId{1};

        void cancelAllPendingGuiRequests();
    };
}
