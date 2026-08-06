#pragma once

#include "TaskGraph_global.h"
#include "Task.h"

#include <QGraphicsObject>
#include <QString>
#include <QColor>

namespace TaskGraph
{
namespace Gui
{
    class TASK_GRAPH_API TaskNodeItem : public QGraphicsObject
    {
        Q_OBJECT
    public:
        TaskNodeItem(const QString& name, const QString& description = QString(),
                     QGraphicsItem* parent = nullptr);

        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void setTaskStatus(Task::Status status);
        Task::Status taskStatus() const { return m_status; }
        QString taskName() const { return m_name; }

        void setPalette(const QColor& border, const QColor& text);
        void setStatusColors(const QColor& pending, const QColor& ready,
                             const QColor& running, const QColor& done,
                             const QColor& failed, const QColor& cancelled,
                             const QColor& skipped);

        QPointF rightCenter() const;
        QPointF leftCenter() const;

    private:
        QColor colorForStatus(Task::Status status) const;

        QString m_name;
        QString m_description;
        Task::Status m_status = Task::Status::Pending;
        static constexpr qreal kWidth = 140.0;
        static constexpr qreal kHeight = 50.0;

        QColor m_border = QColor(0, 0, 0);
        QColor m_text = QColor(0, 0, 0);
        QColor m_statusPending   = QColor(200, 200, 200);
        QColor m_statusReady     = QColor(173, 216, 230);
        QColor m_statusRunning   = QColor(255, 255, 100);
        QColor m_statusDone      = QColor(100, 200, 100);
        QColor m_statusFailed    = QColor(220, 60, 60);
        QColor m_statusCancelled = QColor(120, 120, 120);
        QColor m_statusSkipped   = QColor(210, 180, 140);
    };
}
}
