#pragma once

#include "TaskGraph_global.h"
#include "AbstractReceiver.h"
#include "LogMessage.h"
#include "Filter/LoggerIDFilter.h"
#include <QHash>
#include <QVector>
#include <QObject>
#include <mutex>
#include <functional>

namespace TaskGraph
{
namespace Gui
{
    class TASK_GRAPH_API TaskLogBuffer : public QObject, public Log::AbstractReceiver
    {
        Q_OBJECT
    public:
        explicit TaskLogBuffer(QObject* parent = nullptr);

        void trackLogger(Log::LoggerID id, const QString& taskName);
        QVector<Log::Message> messagesFor(Log::LoggerID id) const;
        QVector<Log::Message> messagesForTask(const QString& taskName) const;
        Log::LoggerID loggerIdForTask(const QString& taskName) const;
        QVector<QString> tasksInOrder() const;
        void clearAll();
        void clearForTask(const QString& taskName);

    signals:
        void messageBuffered(Log::LoggerID loggerId, QString taskName, Log::Message message);

    protected:
        void onNewLogger(Log::LogObject::Info loggerInfo) override;
        void onLoggerInfoChanged(Log::LogObject::Info info) override;
        void onLogMessage(Log::Message message) override;
        void onChangeParent(Log::LoggerID childID, Log::LoggerID newParentID) override;

    private:
        mutable std::mutex m_mutex;
        QHash<Log::LoggerID, QVector<Log::Message>> m_messages;
        QHash<Log::LoggerID, QString> m_idToTask;
        QHash<QString, Log::LoggerID> m_taskToId;
        QVector<QString> m_taskOrder;
    };
}
}
