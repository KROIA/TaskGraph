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

        void setLeaderTarget(const QPointF& sceneCenter, const QRectF& sceneRect);
        void clearLeader();
        void setOverlay(TaskLogOverlay* overlay);

        // Keep the sceneRect padded well beyond the viewport so AnchorUnderMouse
        // always has scroll room (Qt otherwise re-centers content when it fits).
        void ensureSceneRectMargin();
        // Re-pad and center the graph in the viewport (resting framing).
        void frameGraph();
        // Fit the entire graph content into the viewport at maximum zoom.
        void fitGraph();

    signals:
        void nodeSelected(QString name);
        void nodeDoubleClicked(QString name);

    protected:
        void wheelEvent(QWheelEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void drawForeground(QPainter* painter, const QRectF& rect) override;

    private:
        bool m_initialFramed = false;
        bool m_panning = false;
        QPoint m_lastPanPos;

        bool m_leaderActive = false;
        QPointF m_leaderNodeScene;
        QRectF m_leaderNodeSceneRect;
        TaskLogOverlay* m_overlay = nullptr;
    };
}
}
