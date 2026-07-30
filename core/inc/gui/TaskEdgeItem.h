#pragma once

#include "TaskGraph_global.h"

#include <QGraphicsPathItem>
#include <QVector>
#include <QPointF>

namespace TaskGraph
{
namespace Gui
{
    class TaskNodeItem;

    class TASK_GRAPH_API TaskEdgeItem : public QGraphicsPathItem
    {
    public:
        enum class Highlight { None, Incoming, Outgoing };

        TaskEdgeItem(TaskNodeItem* from, TaskNodeItem* to, QGraphicsItem* parent = nullptr);

        void setRoute(const QVector<QPointF>& route);
        void rebuild();

        void setHighlight(Highlight state);
        Highlight highlight() const { return m_highlight; }

        TaskNodeItem* fromNode() const { return m_from; }
        TaskNodeItem* toNode() const { return m_to; }

    private:
        void buildPathFromRoute();
        void applyPen();

        TaskNodeItem* m_from;
        TaskNodeItem* m_to;
        QVector<QPointF> m_route;
        Highlight m_highlight = Highlight::None;
    };
}
}
