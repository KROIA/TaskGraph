#include "gui/TaskLogBuffer.h"

#if defined(QT_WIDGETS_ENABLED)

namespace TaskGraph
{
namespace Gui
{
    TaskLogBuffer::TaskLogBuffer(QObject* parent)
        : QObject(parent)
    {
    }

    void TaskLogBuffer::trackLogger(Log::LoggerID id, const QString& taskName)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_idToTask[id] = taskName;
        m_taskToId[taskName] = id;
        if (!m_messages.contains(id))
            m_messages[id] = {};
    }

    QVector<Log::Message> TaskLogBuffer::messagesFor(Log::LoggerID id) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.value(id);
    }

    QVector<Log::Message> TaskLogBuffer::messagesForTask(const QString& taskName) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Log::LoggerID id = m_taskToId.value(taskName, 0);
        return m_messages.value(id);
    }

    Log::LoggerID TaskLogBuffer::loggerIdForTask(const QString& taskName) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_taskToId.value(taskName, 0);
    }

    QVector<QString> TaskLogBuffer::tasksInOrder() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_taskOrder;
    }

    void TaskLogBuffer::onNewLogger(Log::LogObject::Info)
    {
    }

    void TaskLogBuffer::onLoggerInfoChanged(Log::LogObject::Info)
    {
    }

    void TaskLogBuffer::onLogMessage(Log::Message message)
    {
        Log::LoggerID id = message.getLoggerID();
        QString taskName;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_idToTask.contains(id))
                return;
            taskName = m_idToTask[id];
            m_messages[id].append(message);
            if (!m_taskOrder.contains(taskName))
                m_taskOrder.append(taskName);
        }
        emit messageBuffered(id, taskName, message);
    }

    void TaskLogBuffer::onChangeParent(Log::LoggerID, Log::LoggerID)
    {
    }

    void TaskLogBuffer::clearAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_messages.begin(); it != m_messages.end(); ++it)
            it.value().clear();
        m_taskOrder.clear();
    }

    void TaskLogBuffer::clearForTask(const QString& taskName)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Log::LoggerID id = m_taskToId.value(taskName, 0);
        if (id != 0)
            m_messages[id].clear();
        m_taskOrder.removeAll(taskName);
    }
}
}

#endif
