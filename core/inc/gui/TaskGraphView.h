#pragma once

#include "TaskGraph_global.h"

#include <QGraphicsView>
#include <QPointF>
#include <QString>

namespace TaskGraph
{
namespace Gui
{
    class TaskLogOverlay;

    class TASK_GRAPH_API TaskGraphView : public QGraphicsView
    {
        Q_OBJECT
    public:
        explicit TaskGraphView(QWidget* parent = nullptr);

        void setLeaderTarget(const QString& taskName, const QPointF& sceneCenter, const QRectF& sceneRect);
        void clearLeader();
        void setOverlay(TaskLogOverlay* overlay);

    signals:
        void nodeSelected(QString name);

    protected:
        void wheelEvent(QWheelEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void drawForeground(QPainter* painter, const QRectF& rect) override;

    private:
        bool m_panning = false;
        QPoint m_lastPanPos;

        bool m_leaderActive = false;
        QPointF m_leaderNodeScene;
        QRectF m_leaderNodeSceneRect;
        TaskLogOverlay* m_overlay = nullptr;
    };
}
}
