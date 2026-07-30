#include "gui/GraphLayout.h"

namespace TaskGraph
{
namespace Gui
{
    QHash<QString, QPointF> GraphLayout::compute(const std::vector<TaskList>& taskGraph)
    {
        QHash<QString, QPointF> positions;

        for (size_t layer = 0; layer < taskGraph.size(); ++layer)
        {
            const TaskList& tasks = taskGraph[layer];
            qreal x = static_cast<qreal>(layer) * (nodeWidth + hGap);
            qreal totalHeight = static_cast<qreal>(tasks.size()) * nodeHeight
                              + static_cast<qreal>(tasks.size() > 0 ? tasks.size() - 1 : 0) * vGap;
            qreal startY = -totalHeight / 2.0;

            for (size_t row = 0; row < tasks.size(); ++row)
            {
                qreal y = startY + static_cast<qreal>(row) * (nodeHeight + vGap);
                QString name = QString::fromStdString(tasks[row]->getName());
                positions.insert(name, QPointF(x, y));
            }
        }

        return positions;
    }
}
}
