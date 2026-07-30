#include "gui/TaskGraphView.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>

namespace TaskGraph
{
namespace Gui
{
    TaskGraphView::TaskGraphView(QWidget* parent)
        : QGraphicsView(parent)
    {
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::NoDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    void TaskGraphView::wheelEvent(QWheelEvent* event)
    {
        double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        scale(factor, factor);
    }

    void TaskGraphView::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton)
        {
            m_panning = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        QGraphicsView::mousePressEvent(event);

        // check selection for signal
        auto items = scene()->selectedItems();
        if (!items.isEmpty())
        {
            auto* node = dynamic_cast<TaskNodeItem*>(items.first());
            if (node)
                emit nodeSelected(node->taskName());
        }
    }

    void TaskGraphView::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_panning)
        {
            QPoint delta = event->pos() - m_lastPanPos;
            m_lastPanPos = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void TaskGraphView::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton && m_panning)
        {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
}
}

#endif
