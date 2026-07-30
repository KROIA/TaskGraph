#pragma once

#include "TaskGraph_global.h"
#include "TaskScheduler.h"
#include <QHash>
#include <QString>
#include <QPointF>
#include <vector>

namespace TaskGraph
{
namespace Gui
{
    class TASK_GRAPH_API GraphLayout
    {
    public:
        static constexpr qreal nodeWidth = 140.0;
        static constexpr qreal nodeHeight = 50.0;
        static constexpr qreal hGap = 60.0;
        static constexpr qreal vGap = 30.0;

        static QHash<QString, QPointF> compute(const std::vector<TaskList>& taskGraph);
    };
}
}
