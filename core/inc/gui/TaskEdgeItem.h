#pragma once

#include "TaskGraph_global.h"

#include <QGraphicsPathItem>

namespace TaskGraph
{
namespace Gui
{
    class TaskNodeItem;

    class TASK_GRAPH_API TaskEdgeItem : public QGraphicsPathItem
    {
    public:
        TaskEdgeItem(TaskNodeItem* from, TaskNodeItem* to, QGraphicsItem* parent = nullptr);
        void rebuild();

    private:
        TaskNodeItem* m_from;
        TaskNodeItem* m_to;
    };
}
}
