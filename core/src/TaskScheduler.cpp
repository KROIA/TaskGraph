#include "TaskScheduler.h"
#include "TaskGraphLogger.h"
#include "CrashReport.h"
#include "LogObject.h"
#include <QMetaObject>
#include <QMetaType>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <thread>

namespace TaskGraph
{
    namespace
    {
        struct RunningGuard
        {
            std::atomic<bool>& flag;
            bool armed = true;
            explicit RunningGuard(std::atomic<bool>& f) : flag(f) {}
            void disarm() { armed = false; }
            ~RunningGuard()
            {
                if (armed)
                    flag.store(false, std::memory_order_release);
            }
        };
    }

    TaskScheduler::TaskScheduler(size_t threadCount)
        : QObject()
        , m_stopThreads(false)
        , m_busyThreads(0)
        , m_isRunning(false)
        , m_paused(false)
        , m_cancelRequested(false)
        , m_aborting(false)
        , m_desiredThreadCount(threadCount)
        , m_progress(0)
        , m_progressF(0.0f)
        , m_totalTasks(0)
        , m_remaining(0)
        , m_weightSum(0.0)
        , m_completedWeight(0.0)
        , m_lastError(Error::noError)
        , m_failurePolicy(FailurePolicy::FailFast)
    {
        m_logger = std::make_unique<Log::LogObject>(std::string("TaskScheduler"));
    }

    Log::LogObject& TaskScheduler::logger() { return *m_logger; }

    int TaskScheduler::allocateGuiRequestId()
    {
        return m_nextGuiRequestId.fetch_add(1, std::memory_order_acq_rel);
    }

    QVariant TaskScheduler::waitForGuiResponse(int requestId, Task* task, const QVariant& payload)
    {
        auto req = std::make_shared<PendingGuiRequest>();
        {
            std::lock_guard<std::mutex> lock(m_pendingGuiMutex);
            m_pendingGuiRequests[requestId] = req;
        }

        const QString taskName = task ? QString::fromStdString(task->getName()) : QString();
        emit guiEventRequested(requestId, taskName, payload);

        std::unique_lock<std::mutex> lk(req->m);
        // Cooperative poll: if the task/graph is cancelled we exit without a value.
        while (!req->ready && !req->cancelled)
        {
            if (m_cancelRequested.load(std::memory_order_acquire)
                || (task && task->isCancelRequested()))
            {
                req->cancelled = true;
                break;
            }
            req->cv.wait_for(lk, std::chrono::milliseconds(100));
        }
        const bool wasCancelled = req->cancelled;
        QVariant out = req->ready ? req->response : QVariant();
        lk.unlock();

        {
            std::lock_guard<std::mutex> lock(m_pendingGuiMutex);
            m_pendingGuiRequests.erase(requestId);
        }
        (void)wasCancelled;
        return out;
    }

    void TaskScheduler::respondToGuiEvent(int requestId, const QVariant& response)
    {
        std::shared_ptr<PendingGuiRequest> req;
        {
            std::lock_guard<std::mutex> lock(m_pendingGuiMutex);
            auto it = m_pendingGuiRequests.find(requestId);
            if (it == m_pendingGuiRequests.end())
            {
                if (m_logger) m_logger->logWarning("respondToGuiEvent: unknown or already-cancelled request id "
                                                  + std::to_string(requestId));
                return;
            }
            req = it->second;
        }
        {
            std::lock_guard<std::mutex> lk(req->m);
            req->response = response;
            req->ready = true;
        }
        req->cv.notify_one();
    }

    void TaskScheduler::cancelAllPendingGuiRequests()
    {
        std::lock_guard<std::mutex> lock(m_pendingGuiMutex);
        for (auto& kv : m_pendingGuiRequests)
        {
            auto& req = kv.second;
            {
                std::lock_guard<std::mutex> lk(req->m);
                req->cancelled = true;
            }
            req->cv.notify_all();
        }
    }

    TaskScheduler::~TaskScheduler()
    {
        std::shared_ptr<std::thread> async;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            async = m_asyncThread;
            m_asyncThread.reset();
        }
        if (async && async->joinable())
            async->join();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopThreads = true;
        }
        m_cvTask.notify_all();
        for (auto& t : m_threads)
        {
            if (t && t->joinable())
                t->join();
        }
        m_threads.clear();
        m_threadExit.clear();
    }

    void TaskScheduler::ensureThreadsSpawned()
    {
        if (m_desiredThreadCount == 0)
            return;
        if (!m_threads.empty())
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopThreads = false;
        }
        for (size_t i = 0; i < m_desiredThreadCount; ++i)
        {
            auto exitFlag = std::make_shared<std::atomic<bool>>(false);
            m_threadExit.push_back(exitFlag);
            m_threads.push_back(std::make_shared<std::thread>(taskThreadFunction, this, static_cast<int>(i), exitFlag));
        }
    }

    bool TaskScheduler::enableThreads(size_t threadCount)
    {
        m_lastError.store(Error::noError, std::memory_order_release);
        if (m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Cannot enable threads while the TaskScheduler is running");
            m_lastError.store(Error::busy, std::memory_order_release);
            return false;
        }

        m_desiredThreadCount = threadCount;
        const size_t current = m_threads.size();
        if (threadCount == current)
            return true;

        if (threadCount > current)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopThreads = false;
            }
            for (size_t i = current; i < threadCount; ++i)
            {
                auto exitFlag = std::make_shared<std::atomic<bool>>(false);
                m_threadExit.push_back(exitFlag);
                m_threads.push_back(std::make_shared<std::thread>(taskThreadFunction, this, static_cast<int>(i), exitFlag));
            }
            return true;
        }

        // Shrink: signal only the surplus threads (from the back) to exit.
        const size_t toStop = current - threadCount;
        std::vector<std::shared_ptr<std::thread>> stopping;
        stopping.reserve(toStop);
        for (size_t i = 0; i < toStop; ++i)
        {
            m_threadExit.back()->store(true, std::memory_order_release);
            stopping.push_back(m_threads.back());
            m_threadExit.pop_back();
            m_threads.pop_back();
        }
        m_cvTask.notify_all();
        for (auto& t : stopping)
        {
            if (t && t->joinable())
                t->join();
        }
        return true;
    }

    bool TaskScheduler::disableThreads()
    {
        m_lastError.store(Error::noError, std::memory_order_release);
        if (m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Cannot disable threads while the TaskScheduler is running");
            m_lastError.store(Error::busy, std::memory_order_release);
            return false;
        }
        if (m_threads.empty())
        {
            m_desiredThreadCount = 0;
            return true;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopThreads = true;
        }
        m_cvTask.notify_all();
        for (auto& thread : m_threads)
        {
            if (thread && thread->joinable())
                thread->join();
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopThreads = false;
        }
        m_threads.clear();
        m_threadExit.clear();
        m_desiredThreadCount = 0;
        return true;
    }

    void TaskScheduler::pause()
    {
        bool expected = false;
        if (!m_paused.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            return;
        if (m_logger) m_logger->logInfo("Graph paused");
        emit paused();
    }

    void TaskScheduler::resume()
    {
        bool expected = true;
        if (!m_paused.compare_exchange_strong(expected, false,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            return;
        m_cvTask.notify_all();
        if (m_logger) m_logger->logInfo("Graph resumed");
        emit resumed();
    }

    bool TaskScheduler::addTask(const std::shared_ptr<Task>& task)
    {
        m_lastError.store(Error::noError, std::memory_order_release);
        if (m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Cannot add task while the TaskScheduler is running");
            m_lastError.store(Error::busy, std::memory_order_release);
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& t : m_allTasks)
        {
            if (t == task)
            {
                Internal::TaskGraphLogger::logError("Task already added to the TaskScheduler");
                m_lastError.store(Error::taskAlreadyAdded, std::memory_order_release);
                return false;
            }
        }
        m_allTasks.push_back(task);
        m_taskGraph.clear();
        return true;
    }

    TaskScheduler::Error TaskScheduler::buildTaskGraph(std::vector<TaskList> &taskGraph) const
    {
        TG_SCHEDULER_PROFILING_FUNCTION(TG_COLOR_STAGE_2);
        struct Node
        {
            std::shared_ptr<Task> task = nullptr;
            bool isVisited = false;
        };

        m_lastError.store(Error::noError, std::memory_order_release);
        std::unordered_map<Task*, Node> nodes;
        nodes.reserve(m_allTasks.size());
        for (const auto& task : m_allTasks)
            nodes[task.get()].task = task;

        for (const auto& task : m_allTasks)
        {
            for (const auto& dependency : task->getDependencies())
            {
                if (!dependency || nodes.find(dependency.get()) == nodes.end())
                {
                    Internal::TaskGraphLogger::logError("A dependency of \"" + task->getName() + "\" not found in the list of tasks");
                    m_lastError.store(Error::missingDependency, std::memory_order_release);
                    return Error::missingDependency;
                }
            }
        }

        taskGraph.clear();

        while (true)
        {
            bool allVisited = true;
            for (auto& node : nodes)
            {
                if (!node.second.isVisited) { allVisited = false; break; }
            }
            if (allVisited)
                break;

            TaskList layer;
            for (auto& node : nodes)
            {
                if (node.second.isVisited)
                    continue;
                auto deps = node.second.task->getDependencies();
                bool allDepsVisited = std::all_of(deps.begin(), deps.end(),
                    [&nodes](const std::shared_ptr<Task>& dep)
                    {
                        auto it = nodes.find(dep.get());
                        return it != nodes.end() && it->second.isVisited;
                    });
                if (allDepsVisited)
                    layer.push_back(node.second.task);
            }
            if (layer.empty())
            {
                Internal::TaskGraphLogger::logError("Dependency graph is not directed acyclic. (Circular dependency)");
                m_lastError.store(Error::dependencyGraphNotDAG, std::memory_order_release);
                return Error::dependencyGraphNotDAG;
            }
            for (auto& t : layer)
                nodes[t.get()].isVisited = true;
            taskGraph.push_back(std::move(layer));
        }
        return Error::noError;
    }

    void TaskScheduler::runTasks()
    {
        TG_SCHEDULER_PROFILING_FUNCTION(TG_COLOR_STAGE_1);
        m_lastError.store(Error::noError, std::memory_order_release);

        bool expected = false;
        if (!m_isRunning.compare_exchange_strong(expected, true,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Task scheduler is already running");
            m_lastError.store(Error::alreadyRunning, std::memory_order_release);
            return;
        }
        runTasksBody();
    }

    void TaskScheduler::runTasksBody()
    {
        RunningGuard guard(m_isRunning);

        ensureThreadsSpawned();

        {
            std::vector<TaskList> layered;
            Error err = buildTaskGraph(layered);
            if (err != Error::noError)
            {
                Internal::TaskGraphLogger::logError("Error building task graph: " + std::to_string(err));
                m_lastError.store(err, std::memory_order_release);
                return;
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_taskGraph = std::move(layered);
        }

        if (m_allTasks.empty())
        {
            Internal::TaskGraphLogger::logWarning("No tasks to run");
            m_lastError.store(Error::noTasks, std::memory_order_release);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_inDegree.clear();
            m_dependents.clear();
            m_aliveByPtr.clear();
            m_readyQueue.clear();
            m_aborting = false;
            m_cancelRequested.store(false, std::memory_order_release);
            m_totalTasks = m_allTasks.size();
            m_remaining = m_totalTasks;
            m_weightSum = 0.0;
            m_completedWeight = 0.0;

            for (const auto& t : m_allTasks)
            {
                m_aliveByPtr[t.get()] = t;
                m_weightSum += t->getWeight();
            }

            for (const auto& t : m_allTasks)
            {
                auto deps = t->getDependencies();
                m_inDegree[t.get()] = static_cast<int>(deps.size());
                for (const auto& d : deps)
                    m_dependents[d.get()].push_back(t.get());
            }

            for (const auto& t : m_allTasks)
            {
                if (m_inDegree[t.get()] == 0)
                    m_readyQueue.push_back(t);
            }
        }

        emit started();
        if (m_logger) m_logger->logInfo("Graph run started (" + std::to_string(m_totalTasks) + " tasks)");
        emit statusMessage(QStringLiteral("Executing tasks"));
        m_progress = 0;
        m_progressF.store(0.0f, std::memory_order_release);
        emit progressUpdate(m_progress);
        emit progressChangedF(0.0f);

        m_cvTask.notify_all();

        if (m_threads.empty())
        {
            while (true)
            {
                std::shared_ptr<Task> next;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_readyQueue.empty())
                        break;
                    next = m_readyQueue.front();
                    m_readyQueue.pop_front();
                }
                // Sync mode: no watchdog thread arming; run inline with retries.
                int attempts = 0;
                TaskContext ctx(next.get(), this);
                while (true)
                {
                    armWatchdog(next);
                    next->runTask(&ctx);
                    if (next->getStatus() == Task::Status::Failed
                        && attempts < next->getMaxRetries()
                        && !m_cancelRequested.load(std::memory_order_acquire))
                    {
                        ++attempts;
                        next->logger().logWarning("Task retry attempt " + std::to_string(attempts));
                        auto backoff = next->getRetryBackoff();
                        if (backoff.count() > 0)
                            std::this_thread::sleep_for(backoff);
                        next->prepareRetry();
                        continue;
                    }
                    break;
                }
                onTaskCompleted(next);
            }
        }
        else
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cvComplete.wait(lock, [this] { return m_remaining == 0; });
        }

        m_progress = 100;
        m_progressF.store(1.0f, std::memory_order_release);
        emit progressUpdate(m_progress);
        emit progressChangedF(1.0f);

        const bool wasCancelled = m_cancelRequested.load(std::memory_order_acquire);
        guard.disarm();
        m_isRunning.store(false, std::memory_order_release);
        if (m_logger) m_logger->logInfo(wasCancelled ? "Graph run ended (cancelled)" : "Graph run completed");
        if (wasCancelled)
            emit cancelled();
        emit completed();
    }

    void TaskScheduler::runTasksAsync()
    {
        m_lastError.store(Error::noError, std::memory_order_release);

        // Flip m_isRunning to true *synchronously* so that any caller polling
        // isRunning() immediately after runTasksAsync() observes the scheduler
        // as running. Without this, the async thread had not yet executed
        // runTasks()'s compare_exchange, so a fast poller could see false and
        // proceed as if the scheduler had already finished.
        bool expected = false;
        if (!m_isRunning.compare_exchange_strong(expected, true,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire))
        {
            m_lastError.store(Error::alreadyRunning, std::memory_order_release);
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_asyncThread && m_asyncThread->joinable())
            m_asyncThread->join();

        m_asyncThread = std::make_shared<std::thread>([this]
        {
            TG_SCHEDULER_PROFILING_THREAD("AsyncThread");
            runTasksBody();
        });
    }

    void TaskScheduler::cancel()
    {
        m_cancelRequested.store(true, std::memory_order_release);
        if (m_logger) m_logger->logWarning("Graph cancel requested");
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& t : m_allTasks)
                t->cancel();
            m_aborting = true;
            cancelPendingLocked();
        }
        cancelAllPendingGuiRequests();
        m_cvTask.notify_all();
        m_cvComplete.notify_all();
    }

    void TaskScheduler::cancelPendingLocked()
    {
        while (!m_readyQueue.empty())
        {
            auto t = m_readyQueue.front();
            m_readyQueue.pop_front();
            if (t->getStatus() == Task::Status::Cancelled)
            {
                if (m_remaining > 0) --m_remaining;
                m_completedWeight += t->getWeight();
                emit taskFinished(QString::fromStdString(t->getName()));
            }
        }
        for (const auto& t : m_allTasks)
        {
            if (t->getStatus() == Task::Status::Cancelled)
            {
                auto it = m_inDegree.find(t.get());
                if (it != m_inDegree.end() && it->second >= 0)
                {
                    it->second = -1;
                    if (m_remaining > 0) --m_remaining;
                    m_completedWeight += t->getWeight();
                }
            }
        }
    }

    void TaskScheduler::skipDescendantsLocked(Task* root)
    {
        std::unordered_set<Task*> visited;
        std::vector<Task*> stack;
        auto it = m_dependents.find(root);
        if (it == m_dependents.end())
            return;
        for (Task* d : it->second)
            stack.push_back(d);

        while (!stack.empty())
        {
            Task* cur = stack.back();
            stack.pop_back();
            if (!visited.insert(cur).second)
                continue;
            auto alive = m_aliveByPtr.find(cur);
            if (alive == m_aliveByPtr.end())
                continue;
            const auto& sp = alive->second;
            if (sp->getStatus() == Task::Status::Pending
                || sp->getStatus() == Task::Status::Ready)
            {
                sp->skip();
                auto idIt = m_inDegree.find(cur);
                if (idIt != m_inDegree.end() && idIt->second >= 0)
                {
                    idIt->second = -1;
                    if (m_remaining > 0) --m_remaining;
                    m_completedWeight += sp->getWeight();
                }
                emit taskFinished(QString::fromStdString(sp->getName()));
            }
            auto depIt = m_dependents.find(cur);
            if (depIt != m_dependents.end())
            {
                for (Task* d : depIt->second)
                    stack.push_back(d);
            }
        }
    }

    void TaskScheduler::onTaskCompleted(const std::shared_ptr<Task>& task)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        const Task::Status status = task->getStatus();
        const QString name = QString::fromStdString(task->getName());

        auto idIt = m_inDegree.find(task.get());
        const bool alreadyAccounted = (idIt != m_inDegree.end() && idIt->second < 0);
        if (idIt != m_inDegree.end())
            idIt->second = -1;

        auto accountWeight = [&]()
        {
            if (!alreadyAccounted)
                m_completedWeight += task->getWeight();
        };

        if (status == Task::Status::Done)
        {
            auto depIt = m_dependents.find(task.get());
            if (depIt != m_dependents.end())
            {
                for (Task* dep : depIt->second)
                {
                    auto degIt = m_inDegree.find(dep);
                    if (degIt == m_inDegree.end() || degIt->second < 0)
                        continue;
                    if (--degIt->second == 0)
                    {
                        auto alive = m_aliveByPtr.find(dep);
                        if (alive != m_aliveByPtr.end()
                            && alive->second->getStatus() == Task::Status::Pending)
                        {
                            m_readyQueue.push_back(alive->second);
                        }
                    }
                }
            }
            if (!alreadyAccounted && m_remaining > 0) --m_remaining;
            accountWeight();
            emit taskFinished(name);
        }
        else if (status == Task::Status::Failed)
        {
            if (!alreadyAccounted && m_remaining > 0) --m_remaining;
            accountWeight();
            const QString err = task->getLastError();
            emit taskFailed(name, err);
            emit errorRaised(QStringLiteral("Task \"") + name + QStringLiteral("\" failed: ") + err);

            if (m_failurePolicy.load(std::memory_order_acquire) == FailurePolicy::FailFast)
            {
                m_aborting = true;
                for (const auto& t : m_allTasks)
                {
                    if (t->getStatus() == Task::Status::Pending)
                        t->cancel();
                }
                cancelPendingLocked();
            }
            else
            {
                skipDescendantsLocked(task.get());
            }
        }
        else if (status == Task::Status::Cancelled)
        {
            if (!alreadyAccounted && m_remaining > 0) --m_remaining;
            accountWeight();
            emit taskFinished(name);
        }
        else if (status == Task::Status::Skipped)
        {
            if (!alreadyAccounted && m_remaining > 0) --m_remaining;
            accountWeight();
            emit taskFinished(name);
        }

        float progF = 0.0f;
        if (m_weightSum > 0.0)
            progF = static_cast<float>(m_completedWeight / m_weightSum);
        if (progF > 1.0f) progF = 1.0f;
        if (progF < 0.0f) progF = 0.0f;
        m_progressF.store(progF, std::memory_order_release);
        const int progInt = static_cast<int>(std::round(progF * 100.0f));
        m_progress.store(progInt, std::memory_order_release);

        lock.unlock();
        emit progressUpdate(progInt);
        emit progressChangedF(progF);
        m_cvTask.notify_all();
        m_cvComplete.notify_all();
    }

    bool TaskScheduler::addDynamicTask(const std::shared_ptr<Task>& child, Task* parent)
    {
        if (!child || !parent)
            return false;
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("addDynamicTask: scheduler not running");
            return false;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_aliveByPtr.find(parent) == m_aliveByPtr.end())
        {
            Internal::TaskGraphLogger::logError("addDynamicTask: parent task not tracked");
            return false;
        }
        if (m_aliveByPtr.find(child.get()) != m_aliveByPtr.end())
        {
            Internal::TaskGraphLogger::logError("addDynamicTask: child task already tracked");
            return false;
        }

        auto declaredDeps = child->getDependencies();
        for (const auto& d : declaredDeps)
        {
            if (!d || m_aliveByPtr.find(d.get()) == m_aliveByPtr.end())
            {
                Internal::TaskGraphLogger::logError("addDynamicTask: child has unresolved dependency");
                return false;
            }
        }

        m_allTasks.push_back(child);
        m_aliveByPtr[child.get()] = child;

        int pendingDeps = 0;
        for (const auto& d : declaredDeps)
        {
            if (d->getStatus() != Task::Status::Done)
            {
                ++pendingDeps;
                m_dependents[d.get()].push_back(child.get());
            }
        }
        m_inDegree[child.get()] = pendingDeps;

        ++m_totalTasks;
        ++m_remaining;
        m_weightSum += child->getWeight();

        if (pendingDeps == 0)
            m_readyQueue.push_back(child);

        const QString msg = QStringLiteral("Spawned dynamic task \"")
                          + QString::fromStdString(child->getName())
                          + QStringLiteral("\"");
        lock.unlock();

        emit statusMessage(msg);
        m_cvTask.notify_all();
        return true;
    }

    void TaskScheduler::armWatchdog(const std::shared_ptr<Task>& task)
    {
        auto to = task->getTimeout();
        if (to.count() <= 0)
            return;
        // Detached per-task watchdog. Uses weak_ptr + timeout generation to
        // no-op if the task completed early or was reset before firing.
        std::weak_ptr<Task> weak = task;
        uint64_t gen = task->beginTimeoutWindow();
        std::thread([weak, to, gen]()
        {
            std::this_thread::sleep_for(to);
            if (auto sp = weak.lock())
            {
                if (sp->currentTimeoutWindow() == gen
                    && sp->getStatus() == Task::Status::Running)
                {
                    sp->signalTimeout();
                }
            }
        }).detach();
    }

    void TaskScheduler::resetTasks()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Cannot reset tasks while running");
            m_lastError.store(Error::busy, std::memory_order_release);
            return;
        }
        TaskList snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot = m_allTasks;
        }
        for (const auto& task : snapshot)
            task->reset();
        emit wasReset();
    }

    void TaskScheduler::clear()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            Internal::TaskGraphLogger::logError("Cannot clear while running");
            m_lastError.store(Error::busy, std::memory_order_release);
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allTasks.clear();
        m_taskGraph.clear();
        m_inDegree.clear();
        m_dependents.clear();
        m_aliveByPtr.clear();
        m_readyQueue.clear();
    }

    unsigned int TaskScheduler::getBusyThreadCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_busyThreads;
    }

    size_t TaskScheduler::getTotalTasks() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalTasks;
    }

    std::vector<TaskList> TaskScheduler::getTaskGraph() const
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_taskGraph.empty())
                return m_taskGraph;
        }
        std::vector<TaskList> taskGraph;
        buildTaskGraph(taskGraph);
        return taskGraph;
    }

    bool TaskContext::spawn(const std::shared_ptr<Task>& child)
    {
        if (!m_scheduler || !m_task)
            return false;
        return m_scheduler->addDynamicTask(child, m_task);
    }

    void TaskScheduler::taskThreadFunction(TaskScheduler* obj, int threadIndex, std::shared_ptr<std::atomic<bool>> localExit)
    {
        TG_SCHEDULER_PROFILING_THREAD(std::string("TaskThread[" + std::to_string(threadIndex) + "]").c_str());
        STACK_WATCHER_FUNC;
        (void)threadIndex;

        while (true)
        {
            std::shared_ptr<Task> currentTask;
            {
                TG_SCHEDULER_PROFILING_BLOCK("WaitForTask", TG_COLOR_STAGE_2);
                std::unique_lock<std::mutex> lock(obj->m_mutex);
                obj->m_cvTask.wait(lock, [obj, &localExit] {
                    return obj->m_stopThreads
                        || localExit->load(std::memory_order_acquire)
                        || (!obj->m_paused.load(std::memory_order_acquire)
                            && !obj->m_readyQueue.empty());
                });
                if (localExit->load(std::memory_order_acquire))
                    break;
                if (obj->m_stopThreads && obj->m_readyQueue.empty())
                    break;
                if (obj->m_paused.load(std::memory_order_acquire))
                    continue;
                if (obj->m_readyQueue.empty())
                    continue;
                currentTask = obj->m_readyQueue.front();
                obj->m_readyQueue.pop_front();
                ++obj->m_busyThreads;
            }

            if (currentTask->getAffinity() == Task::TaskAffinity::Gui)
            {
                std::shared_ptr<Task> t = currentTask;
                TaskScheduler* self = obj;
                QMetaObject::invokeMethod(t.get(), [t, self]() {
                    TaskContext ctx(t.get(), self);
                    self->armWatchdog(t);
                    t->runTask(&ctx);
                    self->onTaskCompleted(t);
                }, Qt::QueuedConnection);
            }
            else
            {
                emit obj->taskStarted(QString::fromStdString(currentTask->getName()));
                TG_GENERAL_PROFILING_NONSCOPED_BLOCK("Process task", TG_COLOR_STAGE_2);

                int attempts = 0;
                TaskContext ctx(currentTask.get(), obj);
                while (true)
                {
                    obj->armWatchdog(currentTask);
                    currentTask->runTask(&ctx);
                    if (currentTask->getStatus() == Task::Status::Failed
                        && attempts < currentTask->getMaxRetries()
                        && !obj->m_cancelRequested.load(std::memory_order_acquire))
                    {
                        ++attempts;
                        currentTask->logger().logWarning("Task retry attempt " + std::to_string(attempts));
                        auto backoff = currentTask->getRetryBackoff();
                        if (backoff.count() > 0)
                            std::this_thread::sleep_for(backoff);
                        currentTask->prepareRetry();
                        continue;
                    }
                    break;
                }

                TG_GENERAL_PROFILING_END_BLOCK;
                obj->onTaskCompleted(currentTask);
            }

            {
                std::unique_lock<std::mutex> lock(obj->m_mutex);
                --obj->m_busyThreads;
            }
        }
    }
}
