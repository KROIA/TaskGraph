#pragma once

#include "TaskGraph_global.h"
#include "Task.h"

#include <QGraphicsObject>
#include <QString>

namespace TaskGraph
{
namespace Gui
{
    class TASK_GRAPH_API TaskNodeItem : public QGraphicsObject
    {
        Q_OBJECT
    public:
        TaskNodeItem(const QString& name, QGraphicsItem* parent = nullptr);

        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void setTaskStatus(Task::Status status);
        Task::Status taskStatus() const { return m_status; }
        QString taskName() const { return m_name; }

        QPointF rightCenter() const;
        QPointF leftCenter() const;

    private:
        static QColor colorForStatus(Task::Status status);

        QString m_name;
        Task::Status m_status = Task::Status::Pending;
        static constexpr qreal kWidth = 140.0;
        static constexpr qreal kHeight = 50.0;
    };
}
}
