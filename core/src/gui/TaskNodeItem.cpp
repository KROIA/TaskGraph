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
        painter->setPen(isSelected() ? QPen(Qt::blue, 2) : QPen(m_border, 1));
        painter->drawRoundedRect(boundingRect(), 8, 8);

        painter->setPen(m_text);
        painter->drawText(boundingRect(), Qt::AlignCenter, m_name);
    }

    void TaskNodeItem::setPalette(const QColor& border, const QColor& text)
    {
        m_border = border;
        m_text = text;
        update();
    }

    void TaskNodeItem::setStatusColors(const QColor& pending, const QColor& ready,
                                       const QColor& running, const QColor& done,
                                       const QColor& failed, const QColor& cancelled,
                                       const QColor& skipped)
    {
        m_statusPending = pending;
        m_statusReady = ready;
        m_statusRunning = running;
        m_statusDone = done;
        m_statusFailed = failed;
        m_statusCancelled = cancelled;
        m_statusSkipped = skipped;
        update();
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

    QColor TaskNodeItem::colorForStatus(Task::Status status) const
    {
        switch (status)
        {
        case Task::Status::Pending:   return m_statusPending;
        case Task::Status::Ready:     return m_statusReady;
        case Task::Status::Running:   return m_statusRunning;
        case Task::Status::Done:      return m_statusDone;
        case Task::Status::Failed:    return m_statusFailed;
        case Task::Status::Cancelled: return m_statusCancelled;
        case Task::Status::Skipped:   return m_statusSkipped;
        }
        return m_statusPending;
    }
}
}

#endif
