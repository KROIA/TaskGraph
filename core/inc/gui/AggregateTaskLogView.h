#pragma once

#include "TaskGraph_global.h"
#include <QWidget>
#include <QString>
#include <QHash>

namespace Log { using LoggerID = unsigned int; class Message; }
class QTreeWidget;
class QTreeWidgetItem;

namespace TaskGraph
{
namespace Gui
{
    class TaskLogBuffer;

    class TASK_GRAPH_API AggregateTaskLogView : public QWidget
    {
        Q_OBJECT
    public:
        explicit AggregateTaskLogView(TaskLogBuffer* buffer,
                                      QWidget* parent = nullptr);

    public slots:
        void clearAll();

    private:
        TaskLogBuffer* m_buffer;
        QTreeWidget* m_tree;
        QHash<QString, QTreeWidgetItem*> m_taskItems;

        QTreeWidgetItem* ensureTaskItem(const QString& taskName);
        QTreeWidgetItem* appendMessage(const QString& taskName, const Log::Message& msg);
    };
}
}
