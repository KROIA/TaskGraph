#pragma once

#include "TaskGraph_base.h"
#include "Task.h"
#include <QObject>
#include <QString>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

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

        signals:
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

        std::shared_ptr<std::thread> m_asyncThread = nullptr;
        mutable std::atomic<Error> m_lastError;
        std::atomic<FailurePolicy> m_failurePolicy;

        static void taskThreadFunction(TaskScheduler *obj, int threadIndex, std::shared_ptr<std::atomic<bool>> localExit);
    };
}
