#include "gui/TaskNodeItem.h"

#if defined(QT_WIDGETS_ENABLED)
#include <QPainter>

namespace TaskGraph
{
namespace Gui
{
    TaskNodeItem::TaskNodeItem(const QString& name, QGraphicsItem* parent)
        : QGraphicsObject(parent)
        , m_name(name)
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
    }

    QRectF TaskNodeItem::boundingRect() const
    {
        return QRectF(0, 0, kWidth, kHeight);
    }

    void TaskNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
    {
        TG_UNUSED(option);
        TG_UNUSED(widget);

        QColor fill = colorForStatus(m_status);
        painter->setBrush(fill);
        painter->setPen(isSelected() ? QPen(Qt::blue, 2) : QPen(Qt::black, 1));
        painter->drawRoundedRect(boundingRect(), 8, 8);

        painter->setPen(Qt::black);
        painter->drawText(boundingRect(), Qt::AlignCenter, m_name);
    }

    void TaskNodeItem::setTaskStatus(Task::Status status)
    {
        if (m_status != status)
        {
            m_status = status;
            update();
        }
    }

    QPointF TaskNodeItem::rightCenter() const
    {
        return pos() + QPointF(kWidth, kHeight / 2.0);
    }

    QPointF TaskNodeItem::leftCenter() const
    {
        return pos() + QPointF(0, kHeight / 2.0);
    }

    QColor TaskNodeItem::colorForStatus(Task::Status status)
    {
        switch (status)
        {
        case Task::Status::Pending:   return QColor(200, 200, 200);
        case Task::Status::Ready:     return QColor(173, 216, 230);
        case Task::Status::Running:   return QColor(255, 255, 100);
        case Task::Status::Done:      return QColor(100, 200, 100);
        case Task::Status::Failed:    return QColor(220, 60, 60);
        case Task::Status::Cancelled: return QColor(120, 120, 120);
        case Task::Status::Skipped:   return QColor(210, 180, 140);
        }
        return QColor(200, 200, 200);
    }
}
}

#endif
