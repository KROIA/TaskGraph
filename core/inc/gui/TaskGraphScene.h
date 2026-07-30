#pragma once

#include "TaskGraph_global.h"
#include "Task.h"

#include <QGraphicsScene>
#include <QHash>
#include <QString>
#include <QList>

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TaskNodeItem;
    class TaskEdgeItem;

    class TASK_GRAPH_API TaskGraphScene : public QGraphicsScene
    {
        Q_OBJECT
    public:
        explicit TaskGraphScene(TaskScheduler* scheduler, QObject* parent = nullptr);

        void rebuild();
        void updateNodeStatus(const QString& name, Task::Status status);
        QPointF nodeCenter(const QString& name) const;
        QRectF nodeSceneRect(const QString& name) const;

        void highlightEdgesForNode(const QString& taskName);
        void clearEdgeHighlights();

    private:
        TaskScheduler* m_scheduler;
        QHash<QString, QList<TaskNodeItem*>> m_nodesByName;
        QList<TaskEdgeItem*> m_edges;
    };
}
}
