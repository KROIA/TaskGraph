#pragma once

#include "TaskGraph_global.h"
#include <QColor>

namespace TaskGraph
{
namespace Gui
{
    enum class ColumnWaypointMode
    {
        TwoEdges,  // left + right column edge waypoints (default)
        OneCenter  // a single center column waypoint
    };

    struct TASK_GRAPH_API GraphVisualConfig
    {
        // routing / interpolation
        ColumnWaypointMode waypointMode = ColumnWaypointMode::TwoEdges;
        bool leftWaypointExact   = true;   // TwoEdges: left column waypoint exact
        bool rightWaypointExact  = true;   // TwoEdges: right column waypoint exact
        bool centerWaypointExact = true;   // OneCenter: center waypoint exact
        bool portAnchorExact     = true;   // blue port anchors exact

        // colors
        QColor background;
        QColor nodeBorder, nodeText;
        QColor statusPending, statusReady, statusRunning, statusDone,
               statusFailed, statusCancelled, statusSkipped; // node fill by status
        QColor edgeLine, edgeArrow, edgeHighlightIncoming, edgeHighlightOutgoing;
        QColor debugWaypoint, debugAnchor;

        static GraphVisualConfig light()
        {
            GraphVisualConfig c;
            c.background      = QColor(255, 255, 255);
            c.nodeBorder      = QColor(0, 0, 0);
            c.nodeText        = QColor(0, 0, 0);
            c.statusPending   = QColor(200, 200, 200);
            c.statusReady     = QColor(173, 216, 230);
            c.statusRunning   = QColor(255, 255, 100);
            c.statusDone      = QColor(100, 200, 100);
            c.statusFailed    = QColor(220, 60, 60);
            c.statusCancelled = QColor(120, 120, 120);
            c.statusSkipped   = QColor(210, 180, 140);
            c.edgeLine        = QColor(Qt::darkGray);
            c.edgeArrow       = QColor(Qt::darkGray);
            c.edgeHighlightIncoming = QColor(80, 140, 220);
            c.edgeHighlightOutgoing = QColor(220, 160, 40);
            c.debugWaypoint   = QColor(220, 40, 40);
            c.debugAnchor     = QColor(40, 90, 220);
            return c;
        }

        static GraphVisualConfig dark()
        {
            GraphVisualConfig c;
            c.background      = QColor(30, 30, 30);
            c.nodeBorder      = QColor(90, 90, 90);
            c.nodeText        = QColor(230, 230, 230);
            c.statusPending   = QColor(90, 90, 90);
            c.statusReady     = QColor(60, 110, 150);
            c.statusRunning   = QColor(170, 150, 40);
            c.statusDone      = QColor(60, 140, 70);
            c.statusFailed    = QColor(170, 50, 50);
            c.statusCancelled = QColor(70, 70, 70);
            c.statusSkipped   = QColor(140, 110, 70);
            c.edgeLine        = QColor(160, 160, 160);
            c.edgeArrow       = QColor(160, 160, 160);
            c.edgeHighlightIncoming = QColor(110, 170, 240);
            c.edgeHighlightOutgoing = QColor(240, 180, 70);
            c.debugWaypoint   = QColor(255, 80, 80);
            c.debugAnchor     = QColor(90, 150, 255);
            return c;
        }
    };
}
}
