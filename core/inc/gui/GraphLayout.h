#pragma once

#include "TaskGraph_global.h"
#include "TaskScheduler.h"
#include "gui/GraphVisualConfig.h"
#include <QHash>
#include <QString>
#include <QPointF>
#include <QVector>
#include <vector>

namespace TaskGraph
{
namespace Gui
{
    struct EdgeRoute
    {
        QString fromName;
        QString toName;
        QVector<QPointF> points;
        QVector<bool> exact; // parallel to points; true = interpolate exactly
    };

    struct LayoutResult
    {
        QHash<QString, QPointF> nodePositions;
        QVector<EdgeRoute> edges;
        QVector<QPointF> dummyPoints;   // layer-crossing waypoint centers
        QVector<QPointF> anchorPoints;  // per-edge horizontal stub anchors
    };

    class TASK_GRAPH_API GraphLayout
    {
    public:
        static constexpr qreal nodeWidth = 140.0;
        static constexpr qreal nodeHeight = 50.0;
        static constexpr qreal vGap = 40.0;

        static constexpr qreal dummySlot = 22.0;
        static constexpr qreal minGap = 60.0;
        static constexpr qreal maxGap = 220.0;
        static constexpr qreal hGap = minGap; // minimum-gap fallback
        static constexpr qreal portMargin = 6.0;
        static constexpr qreal perpLen = 18.0; // perpendicular exit/entry stub length

        static LayoutResult computeLayout(const std::vector<TaskList>& graph,
                                          const GraphVisualConfig& cfg = GraphVisualConfig::light());
    };
}
}
