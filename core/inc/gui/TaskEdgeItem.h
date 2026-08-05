#pragma once

#include "TaskGraph_global.h"

#include <QGraphicsPathItem>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QPainterPath>

class QPainter;
class QStyleOptionGraphicsItem;

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

        void setRoute(const QVector<QPointF>& route, const QVector<bool>& exact = QVector<bool>());
        void rebuild();

        void setHighlight(Highlight state);
        Highlight highlight() const { return m_highlight; }

        void setLineColor(const QColor& c);
        void setArrowColor(const QColor& c);
        void setHighlightColors(const QColor& incoming, const QColor& outgoing);

        TaskNodeItem* fromNode() const { return m_from; }
        TaskNodeItem* toNode() const { return m_to; }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    private:
        void buildPathFromRoute();
        void applyPen();

        TaskNodeItem* m_from;
        TaskNodeItem* m_to;
        QVector<QPointF> m_route;
        QVector<bool> m_exact;
        Highlight m_highlight = Highlight::None;

        QPainterPath m_linePath;
        QPainterPath m_arrowPath;

        QColor m_lineColor = QColor(Qt::darkGray);
        QColor m_arrowColor = QColor(Qt::darkGray);
        QColor m_highlightIncoming = QColor(80, 140, 220);
        QColor m_highlightOutgoing = QColor(220, 160, 40);
    };
}
}
