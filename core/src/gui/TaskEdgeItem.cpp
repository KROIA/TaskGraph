#include "gui/TaskEdgeItem.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include <QPen>
#include <QPainter>
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

    void TaskEdgeItem::setRoute(const QVector<QPointF>& route, const QVector<bool>& exact)
    {
        m_route = route;
        m_exact = exact;
        buildPathFromRoute();
    }

    void TaskEdgeItem::rebuild()
    {
        if (m_route.isEmpty())
        {
            QPointF start = m_from->rightCenter();
            QPointF end = m_to->leftCenter();
            m_route = { start, end };
            m_exact = { true, true };
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

    void TaskEdgeItem::setLineColor(const QColor& c)
    {
        m_lineColor = c;
        applyPen();
        update();
    }

    void TaskEdgeItem::setArrowColor(const QColor& c)
    {
        m_arrowColor = c;
        update();
    }

    void TaskEdgeItem::setHighlightColors(const QColor& incoming, const QColor& outgoing)
    {
        m_highlightIncoming = incoming;
        m_highlightOutgoing = outgoing;
        applyPen();
        update();
    }

    void TaskEdgeItem::applyPen()
    {
        switch (m_highlight)
        {
            case Highlight::Incoming:
                setPen(QPen(m_highlightIncoming, 2.5)); // selected depends on source
                setZValue(1.0);
                break;
            case Highlight::Outgoing:
                setPen(QPen(m_highlightOutgoing, 2.5)); // target depends on selected
                setZValue(1.0);
                break;
            default:
                setPen(QPen(m_lineColor, 1.5));
                setZValue(0.0);
                break;
        }
    }

    void TaskEdgeItem::buildPathFromRoute()
    {
        m_linePath = QPainterPath();
        m_arrowPath = QPainterPath();

        if (m_route.size() < 2)
        {
            setPath(m_linePath);
            return;
        }

        const QVector<QPointF>& P = m_route;
        const int last = P.size() - 1;

        // Exact flags: fall back to all-exact if not supplied / mismatched.
        QVector<bool> E = m_exact;
        if (E.size() != P.size())
            E = QVector<bool>(P.size(), true);
        E[0] = true;
        E[last] = true;

        // Interpolate exact points; approx points steer between them.
        QVector<int> exactIdx;
        for (int i = 0; i < P.size(); ++i)
            if (E[i])
                exactIdx.append(i);

        m_linePath.moveTo(P[exactIdx.first()]);
        for (int s = 0; s + 1 < exactIdx.size(); ++s)
        {
            int a = exactIdx[s];
            int b = exactIdx[s + 1];
            QPointF A = P[a];
            QPointF B = P[b];
            int nCtrl = b - a - 1;

            if (nCtrl == 0)
            {
                qreal mx = (A.x() + B.x()) * 0.5;
                m_linePath.cubicTo(QPointF(mx, A.y()), QPointF(mx, B.y()), B);
            }
            else if (nCtrl == 1)
            {
                m_linePath.quadTo(P[a + 1], B);
            }
            else if (nCtrl == 2)
            {
                m_linePath.cubicTo(P[a + 1], P[a + 2], B);
            }
            else
            {
                m_linePath.cubicTo(P[a + 1], P[b - 1], B);
            }
        }

        // Arrowhead: entry anchor sits at port Y => horizontal => perpendicular.
        qreal arrowSize = 8.0;
        QPointF end = P.last();
        QPointF prev = P[last - 1];
        double angle = std::atan2(-(end.y() - prev.y()), end.x() - prev.x());

        QPointF a1 = end + QPointF(std::cos(angle + M_PI + M_PI / 6) * arrowSize,
                                   -std::sin(angle + M_PI + M_PI / 6) * arrowSize);
        QPointF a2 = end + QPointF(std::cos(angle + M_PI - M_PI / 6) * arrowSize,
                                   -std::sin(angle + M_PI - M_PI / 6) * arrowSize);
        m_arrowPath.moveTo(end);
        m_arrowPath.lineTo(a1);
        m_arrowPath.moveTo(end);
        m_arrowPath.lineTo(a2);

        // Combined path drives bounding rect / shape.
        QPainterPath combined = m_linePath;
        combined.addPath(m_arrowPath);
        setPath(combined);
    }

    void TaskEdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::NoBrush);

        QPen linePen = pen();
        painter->setPen(linePen);
        painter->drawPath(m_linePath);

        QColor arrowColor = m_arrowColor;
        if (m_highlight == Highlight::Incoming)
            arrowColor = m_highlightIncoming;
        else if (m_highlight == Highlight::Outgoing)
            arrowColor = m_highlightOutgoing;

        QPen arrowPen(arrowColor, linePen.widthF());
        painter->setPen(arrowPen);
        painter->drawPath(m_arrowPath);
    }
}
}
#endif
