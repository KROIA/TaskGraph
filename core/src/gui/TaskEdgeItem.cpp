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
        setPen(QPen(Qt::darkGray, 1.5));
        rebuild();
    }

    void TaskEdgeItem::rebuild()
    {
        QPointF start = m_from->rightCenter();
        QPointF end = m_to->leftCenter();

        QPainterPath p;
        p.moveTo(start);
        p.lineTo(end);

        // arrowhead
        qreal arrowSize = 8.0;
        QLineF line(start, end);
        double angle = std::atan2(-line.dy(), line.dx());

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
