#include "gui/TaskEdgeItem.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include <QPen>
#include <QtMath>

namespace TaskGraph
{
namespace Gui
{
    TaskEdgeItem::TaskEdgeItem(TaskNodeItem* from, TaskNodeItem* to, QGraphicsItem* parent)
        : QGraphicsPathItem(parent)
        , m_from(from)
        , m_to(to)
    {
        applyPen();
    }

    void TaskEdgeItem::setRoute(const QVector<QPointF>& route)
    {
        m_route = route;
        buildPathFromRoute();
    }

    void TaskEdgeItem::rebuild()
    {
        if (m_route.isEmpty())
        {
            QPointF start = m_from->rightCenter();
            QPointF end = m_to->leftCenter();
            m_route = { start, end };
        }
        buildPathFromRoute();
    }

    void TaskEdgeItem::setHighlight(Highlight state)
    {
        if (m_highlight != state)
        {
            m_highlight = state;
            applyPen();
            update();
        }
    }

    void TaskEdgeItem::applyPen()
    {
        switch (m_highlight)
        {
            case Highlight::Incoming:
                setPen(QPen(QColor(80, 140, 220), 2.5));   // blue: selected depends on source
                setZValue(1.0);
                break;
            case Highlight::Outgoing:
                setPen(QPen(QColor(220, 160, 40), 2.5));    // amber: target depends on selected
                setZValue(1.0);
                break;
            default:
                setPen(QPen(Qt::darkGray, 1.5));
                setZValue(0.0);
                break;
        }
    }

    void TaskEdgeItem::buildPathFromRoute()
    {
        if (m_route.size() < 2)
            return;

        QPainterPath p;
        p.moveTo(m_route.first());
        for (int i = 1; i < m_route.size(); ++i)
            p.lineTo(m_route[i]);

        // Arrowhead at the last segment, pointing into the target node
        qreal arrowSize = 8.0;
        QPointF end = m_route.last();
        QPointF prev = m_route[m_route.size() - 2];
        QLineF lastSeg(prev, end);
        double angle = std::atan2(-lastSeg.dy(), lastSeg.dx());

        QPointF a1 = end + QPointF(std::cos(angle + M_PI + M_PI / 6) * arrowSize,
                                   -std::sin(angle + M_PI + M_PI / 6) * arrowSize);
        QPointF a2 = end + QPointF(std::cos(angle + M_PI - M_PI / 6) * arrowSize,
                                   -std::sin(angle + M_PI - M_PI / 6) * arrowSize);
        p.moveTo(end);
        p.lineTo(a1);
        p.moveTo(end);
        p.lineTo(a2);

        setPath(p);
    }
}
}
#endif
