#include "Task.h"
#include "TaskScheduler.h"
#include "TaskGraphLogger.h"
#include "CrashReport.h"
#include "LogObject.h"
#include <unordered_set>
#include <exception>

namespace TaskGraph
{
    Task::Task()
        : QObject()
        , m_name("Task")
        , m_status(Status::Pending)
        , m_affinity(TaskAffinity::Any)
        , m_cancelRequested(false)
        , m_timeoutHit(false)
        , m_timeoutGen(0)
        , m_weight(1.0f)
        , m_timeoutMs(0)
        , m_maxRetries(0)
        , m_backoffMs(0)
        , m_logger(std::make_unique<Log::LogObject>(std::string("Task")))
        , m_workFunction(nullptr)
        , m_workFunctionCtx(nullptr)
    {
    }

    Task::Task(const std::string& name)
        : QObject()
        , m_name(name)
        , m_status(Status::Pending)
        , m_affinity(TaskAffinity::Any)
        , m_cancelRequested(false)
        , m_timeoutHit(false)
        , m_timeoutGen(0)
        , m_weight(1.0f)
        , m_timeoutMs(0)
        , m_maxRetries(0)
        , m_backoffMs(0)
        , m_logger(std::make_unique<Log::LogObject>(name))
        , m_workFunction(nullptr)
        , m_workFunctionCtx(nullptr)
    {
    }

    Task::~Task()
    {
    }

    void Task::setName(const std::string& name)
    {
        m_name = name;
        if (m_logger)
            m_logger->setName(name);
    }

    Log::LogObject& Task::logger() { return *m_logger; }
    const Log::LogObject& Task::logger() const { return *m_logger; }

    void Task::setWeight(float w)
    {
        if (!(w > 0.0f))
        {
            Internal::TaskGraphLogger::logError("Task weight must be positive; ignoring");
            return;
        }
        m_weight.store(w, std::memory_order_release);
    }

    bool Task::runTask()
    {
        return runTask(nullptr);
    }

    bool Task::runTask(TaskContext* ctx)
    {
        TG_TASK_PROFILING_BLOCK(m_name.c_str(), TG_COLOR_STAGE_1);
        STACK_WATCHER_FUNC;

        if (m_cancelRequested.load(std::memory_order_acquire))
        {
            Status expected = Status::Pending;
            m_status.compare_exchange_strong(expected, Status::Cancelled,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
            return false;
        }

        std::vector<std::shared_ptr<Task>> deps;
        {
            std::lock_guard<std::mutex> lock(m_depMutex);
            deps.reserve(m_dependencies.size());
            for (const auto& w : m_dependencies)
            {
                auto sp = w.lock();
                if (!sp)
                {
                    Internal::TaskGraphLogger::logError("Task \"" + m_name + "\" has an expired dependency");
                    setLastError(QStringLiteral("Expired dependency"));
                    m_status.store(Status::Failed, std::memory_order_release);
                    emit failed(getLastError());
                    return false;
                }
                deps.push_back(std::move(sp));
            }
        }

        for (const auto& dep : deps)
        {
            if (!dep->isDone())
            {
                Internal::TaskGraphLogger::logError("Task: \"" + m_name + "\" can't run because some dependencies aren't processed");
                return false;
            }
        }

        Status expected = Status::Pending;
        if (!m_status.compare_exchange_strong(expected, Status::Running,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
        {
            if (expected == Status::Done)
                Internal::TaskGraphLogger::logError("Task is already done");
            else if (expected == Status::Running)
                Internal::TaskGraphLogger::logError("Task is already running");
            return false;
        }

        if (m_logger) m_logger->logInfo("Task started");
        emit started();
        try
        {
            if (m_workFunctionCtx && ctx)
                m_workFunctionCtx(*ctx);
            else if (m_workFunction)
                m_workFunction();
            else if (ctx)
                work(*ctx);
            else
                work();
        }
        catch (const std::exception& e)
        {
            const QString msg = QString::fromUtf8(e.what());
            if (m_logger) m_logger->logError(std::string("Task threw: ") + e.what());
            setLastError(msg);
            m_status.store(Status::Failed, std::memory_order_release);
            emit failed(msg);
            return false;
        }
        catch (...)
        {
            const QString msg = QStringLiteral("Unknown exception");
            if (m_logger) m_logger->logError("Task threw an unknown exception");
            setLastError(msg);
            m_status.store(Status::Failed, std::memory_order_release);
            emit failed(msg);
            return false;
        }

        // Timeout takes precedence over cooperative cancel: watchdog set cancel + timeout flag.
        if (m_timeoutHit.load(std::memory_order_acquire))
        {
            const QString msg = QStringLiteral("timeout");
            setLastError(msg);
            m_status.store(Status::Failed, std::memory_order_release);
            if (m_logger) m_logger->logError("Task timed out");
            emit failed(msg);
            return false;
        }

        if (m_cancelRequested.load(std::memory_order_acquire))
        {
            m_status.store(Status::Cancelled, std::memory_order_release);
            if (m_logger) m_logger->logWarning("Task cancelled");
            return false;
        }

        if (m_logger) m_logger->logInfo("Task completed");
        m_status.store(Status::Done, std::memory_order_release);
        emit completed();
        return true;
    }

    void Task::reset()
    {
        m_cancelRequested.store(false, std::memory_order_release);
        m_timeoutHit.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(m_errorMutex);
            m_lastError.clear();
            m_result.reset();
        }
        m_status.store(Status::Pending, std::memory_order_release);
        emit wasReset();
    }

    void Task::prepareRetry()
    {
        m_cancelRequested.store(false, std::memory_order_release);
        m_timeoutHit.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(m_errorMutex);
            m_lastError.clear();
            m_result.reset();
        }
        m_status.store(Status::Pending, std::memory_order_release);
    }

    void Task::cancel()
    {
        m_cancelRequested.store(true, std::memory_order_release);
        Status expected = Status::Pending;
        if (m_status.compare_exchange_strong(expected, Status::Cancelled,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
            return;
        expected = Status::Ready;
        m_status.compare_exchange_strong(expected, Status::Cancelled,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire);
    }

    void Task::skip()
    {
        Status expected = Status::Pending;
        if (m_status.compare_exchange_strong(expected, Status::Skipped,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
            return;
        expected = Status::Ready;
        m_status.compare_exchange_strong(expected, Status::Skipped,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire);
    }

    void Task::signalTimeout()
    {
        m_timeoutHit.store(true, std::memory_order_release);
        m_cancelRequested.store(true, std::memory_order_release);
    }

    QString Task::getLastError() const
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        return m_lastError;
    }

    void Task::setLastError(const QString& err)
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastError = err;
    }

    std::any Task::getResult() const
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        return m_result;
    }

    void Task::setResult(std::any value)
    {
        if (m_status.load(std::memory_order_acquire) != Status::Running)
        {
            Internal::TaskGraphLogger::logError("Task::setResult called outside Running state");
            return;
        }
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_result = std::move(value);
    }

    bool Task::checkDependencies()
    {
        std::lock_guard<std::mutex> lock(m_depMutex);
        for (const auto& w : m_dependencies)
        {
            auto sp = w.lock();
            if (!sp || !sp->isDone())
                return false;
        }
        return true;
    }

    std::vector<std::shared_ptr<Task>> Task::getDependencies() const
    {
        std::lock_guard<std::mutex> lock(m_depMutex);
        std::vector<std::shared_ptr<Task>> out;
        out.reserve(m_dependencies.size());
        for (const auto& w : m_dependencies)
        {
            if (auto sp = w.lock())
                out.push_back(std::move(sp));
        }
        return out;
    }

    bool Task::wouldCreateCycle(const std::shared_ptr<Task>& candidate) const
    {
        std::unordered_set<const Task*> visited;
        std::vector<std::shared_ptr<Task>> stack;
        stack.push_back(candidate);
        while (!stack.empty())
        {
            auto cur = std::move(stack.back());
            stack.pop_back();
            if (!cur)
                continue;
            if (cur.get() == this)
                return true;
            if (!visited.insert(cur.get()).second)
                continue;
            auto curDeps = cur->getDependencies();
            for (auto& d : curDeps)
                stack.push_back(std::move(d));
        }
        return false;
    }

    bool Task::addDependency(const std::shared_ptr<Task>& task)
    {
        if (isRunning())
        {
            Internal::TaskGraphLogger::logError("Can't add dependencies to a running task");
            return false;
        }
        if (!task)
        {
            Internal::TaskGraphLogger::logError("Can't add null dependency");
            return false;
        }
        if (task.get() == this)
        {
            Internal::TaskGraphLogger::logError("Can't add task as dependency to itself");
            return false;
        }
        if (wouldCreateCycle(task))
        {
            Internal::TaskGraphLogger::logError("Can't add dependency \"" + task->getName() + "\" to \"" + m_name + "\": would create cycle");
            return false;
        }

        std::lock_guard<std::mutex> lock(m_depMutex);
        for (const auto& w : m_dependencies)
        {
            if (w.lock() == task)
                return true;
        }
        m_dependencies.push_back(task);
        return true;
    }

    bool Task::addDependency(const TaskGroup& group)
    {
        bool allOk = true;
        for (const auto& m : group.members())
        {
            if (!addDependency(m))
                allOk = false;
        }
        return allOk;
    }

    bool Task::clearDependencies()
    {
        if (isRunning())
        {
            Internal::TaskGraphLogger::logError("Can't clear dependencies of a running task");
            return false;
        }
        std::lock_guard<std::mutex> lock(m_depMutex);
        m_dependencies.clear();
        return true;
    }

    void Task::work()
    {
        Internal::TaskGraphLogger::logWarning("Task::work() is not implemented");
    }

    void Task::work(TaskContext& /*ctx*/)
    {
        work();
    }

    // ---- TaskContext ----
    TaskContext::~TaskContext() = default;

    void TaskContext::setResult(std::any value)
    {
        if (m_task)
            m_task->setResult(std::move(value));
    }

    bool TaskContext::isCancelRequested() const
    {
        return m_task ? m_task->isCancelRequested() : false;
    }

    Log::LogObject& TaskContext::log()
    {
        return m_task->logger();
    }

    QVariant TaskContext::askGui(const QVariant& payload)
    {
        if (!m_scheduler || !m_task)
            return QVariant();
        const int id = m_scheduler->allocateGuiRequestId();
        return m_scheduler->waitForGuiResponse(id, m_task, payload);
    }
}
