#pragma once

#include "TaskGraph_global.h"

#include <QGraphicsView>

namespace TaskGraph
{
namespace Gui
{
    class TASK_GRAPH_API TaskGraphView : public QGraphicsView
    {
        Q_OBJECT
    public:
        explicit TaskGraphView(QWidget* parent = nullptr);

    signals:
        void nodeSelected(QString name);

    protected:
        void wheelEvent(QWheelEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        bool m_panning = false;
        QPoint m_lastPanPos;
    };
}
}
