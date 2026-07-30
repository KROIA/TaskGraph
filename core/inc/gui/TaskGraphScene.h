#pragma once

#include "TaskGraph_global.h"
#include "Task.h"

#include <QGraphicsScene>
#include <QHash>
#include <QString>

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TaskNodeItem;

    class TASK_GRAPH_API TaskGraphScene : public QGraphicsScene
    {
        Q_OBJECT
    public:
        explicit TaskGraphScene(TaskScheduler* scheduler, QObject* parent = nullptr);

        void rebuild();
        void updateNodeStatus(const QString& name, Task::Status status);

    private:
        TaskScheduler* m_scheduler;
        QHash<QString, QList<TaskNodeItem*>> m_nodesByName;
    };
}
}
