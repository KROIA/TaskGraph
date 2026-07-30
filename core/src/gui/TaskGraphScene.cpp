#include "gui/TaskGraphScene.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include "gui/TaskEdgeItem.h"
#include "gui/GraphLayout.h"
#include "TaskScheduler.h"

namespace TaskGraph
{
namespace Gui
{
    TaskGraphScene::TaskGraphScene(TaskScheduler* scheduler, QObject* parent)
        : QGraphicsScene(parent)
        , m_scheduler(scheduler)
    {
    }

    void TaskGraphScene::rebuild()
    {
        clear();
        m_nodesByName.clear();

        std::vector<TaskList> graph = m_scheduler->getTaskGraph();
        QHash<QString, QPointF> positions = GraphLayout::compute(graph);

        // create nodes
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                QString name = QString::fromStdString(task->getName());
                auto* node = new TaskNodeItem(name);
                if (positions.contains(name))
                    node->setPos(positions.value(name));
                node->setTaskStatus(task->getStatus());
                addItem(node);
                m_nodesByName[name].append(node);
            }
        }

        // create edges: for each task, draw from each dependency to the task
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                QString taskName = QString::fromStdString(task->getName());
                auto deps = task->getDependencies();
                for (const auto& dep : deps)
                {
                    QString depName = QString::fromStdString(dep->getName());
                    if (m_nodesByName.contains(depName) && m_nodesByName.contains(taskName))
                    {
                        TaskNodeItem* fromNode = m_nodesByName[depName].first();
                        TaskNodeItem* toNode = m_nodesByName[taskName].first();
                        auto* edge = new TaskEdgeItem(fromNode, toNode);
                        addItem(edge);
                    }
                }
            }
        }
    }

    void TaskGraphScene::updateNodeStatus(const QString& name, Task::Status status)
    {
        if (m_nodesByName.contains(name))
        {
            for (auto* node : m_nodesByName[name])
                node->setTaskStatus(status);
        }
    }

    QPointF TaskGraphScene::nodeCenter(const QString& name) const
    {
        if (m_nodesByName.contains(name) && !m_nodesByName[name].isEmpty())
        {
            auto* node = m_nodesByName[name].first();
            return node->pos() + node->boundingRect().center();
        }
        return QPointF();
    }

    QRectF TaskGraphScene::nodeSceneRect(const QString& name) const
    {
        if (m_nodesByName.contains(name) && !m_nodesByName[name].isEmpty())
        {
            auto* node = m_nodesByName[name].first();
            return node->boundingRect().translated(node->pos());
        }
        return QRectF();
    }
}
}

#endif
